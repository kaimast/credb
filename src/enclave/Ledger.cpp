#include "Ledger.h"

#include "credb/Transaction.h"
#include "credb/Witness.h"
#include "credb/defines.h"

#ifdef FAKE_ENCLAVE
#include "../server/FakeEnclave.h"
#else
#include "Enclave_t.h"
#include <sgx_tcrypto.h>
#endif

#include <cowlang/Interpreter.h>
#include <cowlang/unpack.h>

#include "Block.h"
#include "Enclave.h"
#include "Index.h"
#include "Witness.h"
#include "LockHandle.h"
#include "PageHandle.h"
#include "Peers.h"
#include "PendingBooleanResponse.h"
#include "Shard.h"
#include "StringIndex.h"
#include "credb/Client.h"
#include "logging.h"

#include "bindings/Database.h"
#include "bindings/Object.h"
#include "bindings/OpContext.h"
#include "bindings/OpInfo.h"

#include <algorithm>

namespace credb
{
namespace trusted
{


Ledger::Ledger(Enclave &enclave)
: m_enclave(enclave), m_buffer_manager(m_enclave.buffer_manager()),
  m_object_count(0), m_version_count(0), m_put_object_index_last_id(0), m_put_object_index_running(false)
{
    for(auto &shard : m_shards)
    {
        shard = new Shard(m_buffer_manager);
        shard->generate_block();
    }
}

Ledger::~Ledger()
{
    m_collections.clear();

    for(auto shard : m_shards)
    {
        delete shard;
    }
}

void Ledger::clear_cached_blocks()
{
    for(auto &shard : m_shards)
    {
        shard->reload_pending_block();
    }
}

bitstream Ledger::prepare_call(const OpContext &op_context,
                               const std::string &collection,
                               const std::string &key,
                               const std::string &path)
{
    LockHandle lock_handle(*this);
    ObjectEventHandle obj;
    event_id_t eid;

    if(!get_latest_version(obj, op_context, collection, key, path, eid, lock_handle, LockType::Read,
                           OperationType::CallProgram))
    {
        log_error("No such object: " + key);
        return bitstream();
    }

    auto val = obj.value(path);

    if(val.get_type() != json::ObjectType::Binary)
    {
        log_error("Not a program: " + key + "." + path);
        return bitstream();
    }

    return val.as_bitstream().duplicate(true);
}

bool Ledger::set_trigger(const std::string &collection, remote_party_id identifier)
{
    // TODO always create collection?
    auto &c = get_collection(collection, true);
    c.set_trigger(identifier);
    return true;
}

void Ledger::remove_triggers_for(remote_party_id identifier)
{
    //FIXME remove linear scan
    for(auto &it: m_collections)
    {
        it.second.unset_trigger(identifier);
    }
}

bool Ledger::unset_trigger(const std::string &collection, remote_party_id identifier)
{
    auto c = try_get_collection(collection);

    if(!c)
    {
        return false;
    }

    c->unset_trigger(identifier);
    return true;
}


bool Ledger::has_object(const std::string &collection, const std::string &key)
{
    event_id_t id;
    auto col = try_get_collection(collection);

    if(!col)
    {
        return false;
    }

    return col->primary_index().get(key, id);
}

uint32_t Ledger::count_writes(const OpContext &op_context,
                              const std::string &user,
                              const std::string &collection,
                              const std::string &key,
                              LockHandle *lock_handle_)
{
    auto it = iterate(op_context, collection, key, lock_handle_);
    uint32_t res = 0;
    ObjectEventHandle hdl;

    while(it.next(hdl))
    {
        if(hdl.source() == user)
        {
            res += 1;
        }
    }

    return res;
}

bool Ledger::get_previous_event(shard_id_t shard_no,
                                ObjectEventHandle &previous,
                                const ObjectEventHandle &current,
                                LockHandle &lock_handle,
                                LockType lock_type)
{
    if(!current.has_predecessor())
    {
        return false;
    }

    auto block = lock_handle.get_block(shard_no, current.previous_block(), lock_type);
    block->get_event(previous, current.previous_index());
    return true;
}

bool Ledger::get_previous_version(shard_id_t shard_no,
                                  ObjectEventHandle &out,
                                  const ObjectEventHandle &current_event,
                                  LockHandle &lock_handle,
                                  LockType lock_type)
{
    block_id_t bid = INVALID_BLOCK;
    ObjectEventHandle next_event;
    next_event = current_event.duplicate();

    while(next_event.valid() && next_event.get_type() != ObjectEventType::NewVersion)
    {
        if(next_event.get_type() == ObjectEventType::Deletion)
        {
            lock_handle.release_block(shard_no, bid, lock_type);
            return false;
        }

        if(!next_event.has_predecessor())
        {
            lock_handle.release_block(shard_no, bid, lock_type);
            return false;
        }

        if(bid == next_event.previous_block())
        {
            auto block = lock_handle.get_block(shard_no, bid, lock_type);
            block->get_event(next_event, next_event.previous_index());
        }
        else
        {
            auto next_bid = next_event.previous_block();
            auto block = lock_handle.get_block(shard_no, next_bid, lock_type);

            block->get_event(next_event, next_event.previous_index());

            lock_handle.release_block(shard_no, bid, lock_type);
            bid = next_bid;
        }
    }

    if(next_event.valid())
    {
        out = std::move(next_event);
        return true;
    }
    else
    {
        return false;
    }
}

bool Ledger::get_event(ObjectEventHandle &out, const event_id_t &eid, LockHandle &lock_handle, LockType lock_type)
{
    auto block = lock_handle.get_block(eid.shard, eid.block, lock_type);
    block->get_event(out, eid.index);
    return true;
}

event_id_t Ledger::add(const OpContext &op_context,
                       const std::string &collection,
                       const std::string &key,
                       json::Document &to_add,
                       const std::string &path,
                       LockHandle *lock_handle_)
{
    if(!Ledger::is_valid_key(key))
    {
        return INVALID_EVENT;
    }

    if(!check_collection_policy(op_context, collection, key, path, OperationType::AddToObject))
    {
        log_debug("rejected add because of collection policy");
        return INVALID_EVENT;
    }

    auto s = get_shard(collection, key);

    LockHandle lock_handle(*this, lock_handle_);

    event_id_t previous_id = INVALID_EVENT;
    version_number_t number = INITIAL_VERSION_NO;

    ObjectEventHandle previous_event, previous_version;

    bool has_latest_version = false;
    bool has_latest_event =
    get_latest_event(previous_event, collection, key, previous_id, lock_handle, LockType::Write);

    if(has_latest_event)
    {
        has_latest_version =
        get_previous_version(s, previous_version, previous_event, lock_handle, LockType::Write);

        if(has_latest_version) // might have been deleted
        {
            number = previous_version.version_number() + 1;
        }
    }

    event_id_t res;

    if(!path.empty() && !has_latest_version)
    {
        // can't update
        res = INVALID_EVENT;
    }
    else if(!has_latest_version)
    {
        res = put_next_version(op_context, collection, key, to_add, number, previous_id,
                               previous_version, lock_handle);
    }
    else
    {
        json::Document view = previous_version.value();
        json::Document policy(view, "policy", false);

        if(!policy.empty() && !check_object_policy(policy, op_context, collection, key, path,
                                                   OperationType::AddToObject, lock_handle))
        {
            log_debug("rejected add because of object policy");
            res = INVALID_EVENT;
        }
        else
        {
            json::Document doc = view.duplicate(true);
            doc.add(path, to_add);

            res = put_next_version(op_context, collection, key, doc, number, previous_id,
                                   previous_version, lock_handle);
        }
    }

    lock_handle.clear();

    if(!lock_handle_)
    {
        organize_ledger(get_shard(collection, key));
    }

    return res;
}

bool Ledger::diff(const OpContext &op_context,
                  const std::string &collection,
                  const std::string &key,
                  json::Diffs &out,
                  version_number_t version1,
                  version_number_t version2)
{
    auto it = iterate(op_context, collection, key);

    ObjectEventHandle hdl1, hdl2;

    while(!(hdl1.valid() && hdl2.valid()))
    {
        ObjectEventHandle h;

        if(!it.next(h))
        {
            return false;
        }

        if(h.version_number() == version1)
        {
            hdl1 = h.duplicate();
        }

        if(h.version_number() == version2)
        {
            hdl2 = h.duplicate();
        }
    }

    if(!hdl1.valid() || !hdl2.valid())
    {
        return false;
    }

    auto val1 = hdl1.value();
    auto val2 = hdl2.value();

    out = val1.diff(val2);
    return true;
}

bool Ledger::create_witness(Witness &witness, const std::vector<event_id_t> &events)
{
    // Create the actual witness content
    // = a digest of changes
    json::Writer writer;
    writer.start_array("");

    for(auto &eid : events)
    {
        LockHandle lock_handle(*this);

        writer.start_map("");
        writer.write_integer(Witness::SHARD_FIELD_NAME, eid.shard);
        writer.write_integer(Witness::BLOCK_FIELD_NAME, eid.block);
        writer.write_integer(Witness::INDEX_FIELD_NAME, eid.index);

        ObjectEventHandle event, prev;

        bool res = get_event(event, eid, lock_handle, LockType::Read);
        if(!res)
        {
            return false;
        }

        bool has_prev = get_previous_event(eid.shard, prev, event, lock_handle, LockType::Read);

        writer.write_string("source", event.source());

        if(event.get_type() == ObjectEventType::Deletion)
        {
            writer.write_string("type", "deletion");
        }
        else if(has_prev && prev.get_type() != ObjectEventType::Deletion)
        {
            auto doc1 = event.value();
            auto doc2 = event.value();
            auto diffs = doc1.diff(doc2);

            writer.write_string("type", "change");

            writer.start_array("diff");
            for(auto &diff : diffs)
            {
                bitstream bstream;
                diff.compress(bstream, true);
                json::Document doc(bstream.data(), bstream.size(), json::DocumentMode::ReadOnly);
            }
            writer.end_array();
        }
        else if(event.get_type() == ObjectEventType::NewVersion)
        {
            writer.write_string("type", "creation");

            json::Document view = event.value();
            writer.write_document("value", view);
        }
        else
        {
            log_error("Unknown pair of object events");
        }

        writer.end_map();
    }
    writer.end_array();

    auto doc = writer.make_document();
    witness.set_data(std::move(doc.data()));
    return sign_witness(m_enclave, witness);
}

bool Ledger::check_collection_policy(const OpContext &op_context,
                                     const std::string &collection,
                                     const std::string &key,
                                     const std::string &path,
                                     OperationType type)
{
    if(!op_context.valid())
    {
        return true;
    }

    // Needed so we don't call the policy recursively
    OpContext empty_context(INVALID_IDENTITY);

    LockHandle lock_handle(*this);

    auto it = iterate(empty_context, collection, "policy", &lock_handle);

    ObjectEventHandle hdl;
    
    if(!it.next(hdl))
    {
        return true;
    }

    bitstream bs = hdl.value().as_bitstream();

    cow::Interpreter pyint(bs);
    auto object_hook = cow::make_value<bindings::Object>(pyint.memory_manager(), empty_context,
                                                         *this, collection, key, lock_handle);
    auto db_hook = cow::make_value<bindings::Database>(pyint.memory_manager(), empty_context, *this,
                                                       m_enclave, nullptr, lock_handle);
    auto op_ctx_hook = cow::make_value<bindings::OpContext>(pyint.memory_manager(), op_context);
    auto op_info_hook = cow::make_value<bindings::OpInfo>(pyint.memory_manager(), type, path);

    
    pyint.set_module("db", db_hook);
    pyint.set_module("op_context", op_ctx_hook);
    pyint.set_module("op_info", op_info_hook);

    try
    {
        return cow::unpack_bool(pyint.execute());
    }
    catch(std::exception &e)
    {
        log_error(std::string("Collection policy failed: ") + e.what());
        return false;
    }
}

bool Ledger::check_object_policy(const json::Document &policy,
                                 const OpContext &op_context,
                                 const std::string &collection,
                                 const std::string &key,
                                 const std::string &path,
                                 OperationType type,
                                 LockHandle &lock_handle)
{
    if(!op_context.valid())
    {
        return true;
    }

    bitstream bs = policy.as_bitstream();

    cow::Interpreter pyint(bs);

    // Needed so we don't call the security policy recursively
    OpContext empty_context(INVALID_IDENTITY);

    auto object_hook = cow::make_value<bindings::Object>(pyint.memory_manager(), empty_context,
                                                         *this, collection, key, lock_handle);
    auto db_hook = cow::make_value<bindings::Database>(pyint.memory_manager(), empty_context, *this,
                                                       m_enclave, nullptr, lock_handle);
    auto op_ctx_hook = cow::make_value<bindings::OpContext>(pyint.memory_manager(), op_context);
    auto op_info_hook = cow::make_value<bindings::OpInfo>(pyint.memory_manager(), type, path);

    pyint.set_module("db", db_hook);
    pyint.set_module("self", object_hook);
    pyint.set_module("op_context", op_ctx_hook);
    pyint.set_module("op_info", op_info_hook);

    try
    {
        return cow::unpack_bool(pyint.execute());
    }
    catch(std::exception &e)
    {
        log_error(std::string("Object policy failed: ") + e.what());
        return false;
    }
}

event_id_t Ledger::put_without_key(const OpContext &op_context,
                       const std::string &collection,
                       std::string &key_out,
                       json::Document &to_put,
                       LockHandle *lock_handle_)
{
    (void)op_context;

    // Acquire lock to the corresponding shard
    // This way we achieve atomicity
    event_id_t eid = INVALID_EVENT;
    shard_id_t shard = -1; 

    while(eid == INVALID_EVENT)
    {
        LockHandle lock_handle(*this, lock_handle_);

        const size_t KEY_LEN = 10;
        const auto key = credb::random_object_key(KEY_LEN);
        shard = get_shard(collection, key);

        // Lock shard
        lock_handle.get_pending_block(shard, LockType::Write);

        ObjectEventHandle prev_hdl;
        event_id_t prev_id;
        bool exists = get_latest_event(prev_hdl, collection, key, prev_id, lock_handle, LockType::Write);
        
        if(!exists)
        {
            eid = put_next_version(op_context, collection, key, to_put, INITIAL_VERSION_NO, prev_id,
                               prev_hdl, lock_handle);
            key_out = key;
        }
    }

    if(lock_handle_ == nullptr)
    {
        organize_ledger(shard);
    }

    return eid;
}

event_id_t Ledger::put(const OpContext &op_context,
                       const std::string &collection,
                       const std::string &key,
                       json::Document &to_put,
                       const std::string &path,
                       LockHandle *lock_handle_)
{
    if(!Ledger::is_valid_key(key))
    {
        return INVALID_EVENT;
    }

    if(!check_collection_policy(op_context, collection, key, path, OperationType::AddToObject))
    {
        log_debug("rejected add because of collection policy");
        return INVALID_EVENT;
    }

    LockHandle lock_handle(*this, lock_handle_);

    auto s = get_shard(collection, key);

    event_id_t res = INVALID_EVENT;
    event_id_t previous_id = INVALID_EVENT;

    version_number_t number = INITIAL_VERSION_NO;

    ObjectEventHandle previous_event, previous_version;
    bool has_previous_version = false;
    bool has_previous_event =
    get_latest_event(previous_event, collection, key, previous_id, lock_handle, LockType::Write);

    if(has_previous_event)
    {
        has_previous_version =
        get_previous_version(s, previous_version, previous_event, lock_handle, LockType::Write);

        if(has_previous_version)
        {
            number = previous_version.version_number() + 1;
        }
    }

    json::Document policy("");

    if(has_previous_version && previous_version.get_policy(policy) &&
       !check_object_policy(policy, op_context, collection, key, path, OperationType::PutObject, lock_handle))
    {
        log_debug("rejected put because of policy");
        res = INVALID_EVENT;
    }
    else if(!path.empty() && !has_previous_version)
    {
        res = INVALID_EVENT;
    }
    else if(!path.empty())
    {
        json::Document view = previous_version.value();
        json::Document doc = view.duplicate(true);
        
        if(doc.insert(path, to_put))
        {
            res = put_next_version(op_context, collection, key, doc, number, previous_id,
                                   previous_version, lock_handle);
        }
    }
    else
    {
        res = put_next_version(op_context, collection, key, to_put, number, previous_id,
                               previous_version, lock_handle);
    }

    lock_handle.clear();

    if(!lock_handle_)
    {
        organize_ledger(s);
    }

    return res;
}

event_id_t Ledger::put_next_version(const OpContext &op_context,
                                    const std::string &collection,
                                    const std::string &key,
                                    json::Document &doc,
                                    version_number_t version_number,
                                    event_id_t previous_id,
                                    const ObjectEventHandle &previous_version,
                                    LockHandle &lock_handle)
{
    if(!op_context.valid())
    {
        throw std::runtime_error("Cannot modify using invalid identity");
    }

    if(!doc.valid())
    {
        log_warning("cannot put invalid document");
        return INVALID_EVENT;
    }

    auto shard_no = get_shard(collection, key);
    auto pending = lock_handle.get_pending_block(shard_no, LockType::Write);

    auto &col = get_collection(collection, true);

    json::Writer writer;

    writer.start_array("");
    writer.write_integer("", static_cast<int32_t>(ObjectEventType::NewVersion));
    writer.write_string("", op_context.to_string());

    if(version_number == INITIAL_VERSION_NO)
    {
        m_object_count++;
        writer.write_integer("", INVALID_BLOCK);
        writer.write_integer("", 0);
    }
    else
    {
        // Remove from indexes first
        // FIXME only remove/update if relevant fields have changed.
        for(auto it : col.secondary_indexes())
        {
            auto index = it.second;
            json::Document view = previous_version.value();
            index->remove(view, key);
        }

        writer.write_integer("", previous_id.block);
        writer.write_integer("", previous_id.index);
    }

    // check if any indexes match
    for(auto it : col.secondary_indexes())
    {
        auto index = it.second;
        index->insert(doc, key);
    }

    writer.write_document("", doc);

    if(previous_version.valid())
    {
        writer.write_integer("", previous_version.version_number() + 1);
    }
    else
    {
        writer.write_integer("", INITIAL_VERSION_NO);
    }

    writer.end_array();

    auto new_version = writer.make_document();

    auto index = pending->insert(new_version);
    pending->flush_page();

    event_id_t event_id = { shard_no, pending->identifier(), index };

    bitstream index_changes;
    index_changes << collection;
    col.primary_index().insert(key, event_id, &index_changes);
#ifndef TEST
    col.notify_triggers(m_enclave.remote_parties());
#endif
    m_version_count += 1;

    ObjectEventHandle version;
    version.assign(std::move(new_version));

    lock_handle.release_block(shard_no, pending->identifier(), LockType::Write);

    // forward only the index to downstream servers
    send_index_updates_to_downstream(index_changes, shard_no, pending->page_no());

    return event_id;
}

void Ledger::send_index_updates_to_downstream(const bitstream &index_changes, shard_id_t shard, page_no_t invalidated_page)
{
#ifdef TEST
    (void)index_changes;
    (void)shard;
    (void)invalidated_page;
#else
    m_put_object_index_mutex.lock();
    size_t id = ++m_put_object_index_last_id;
    m_put_object_index_mutex.unlock();

    for(auto downstream_id : m_enclave.peers().get_downstream_set())
    {
        //        log_debug("forwarding index to downstream_id=" + std::to_string(downstream_id));
        auto peer = m_enclave.peers().find(downstream_id);
        if(!peer)
        {
            log_error("unable to find peer " + std::to_string(downstream_id));
            abort();
        }
        peer->lock();

        bitstream update_msg;
        update_msg << static_cast<mtype_data_t>(MessageType::PushIndexUpdate);
        update_msg << id;
        update_msg << index_changes;
        update_msg << shard;
        update_msg << invalidated_page;
        peer->send(update_msg);
        peer->unlock();
    }
#endif
}

void Ledger::put_object_index_from_upstream(size_t input_id, bitstream *input_changes, shard_id_t input_shard, page_no_t input_block_page_no)
{
    m_put_object_index_mutex.lock();
    m_put_object_index_queue.emplace(input_id, input_changes, input_shard, input_block_page_no);
    if(m_put_object_index_running)
    {
        m_put_object_index_mutex.unlock();
        return;
    }

    m_put_object_index_running = true;
    m_put_object_index_mutex.unlock();

    for(;;)
    {
        m_put_object_index_mutex.lock();
        if(m_put_object_index_queue.empty())
        {
            m_put_object_index_running = false;
            m_put_object_index_mutex.unlock();
            return;
        }

        auto[id, changes, shard_id, block_page_no] = m_put_object_index_queue.top();
        if(m_put_object_index_last_id && id != m_put_object_index_last_id + 1)
        {
            // although TCP guarantees packet order, it can still be reordered at this function.
            //            log_debug("Waiting for Update #" +
            //            std::to_string(m_put_object_index_last_id+1) + " got #" +
            //            std::to_string(id) + ", return.");
            m_put_object_index_running = false;
            m_put_object_index_mutex.unlock();
            return;
        }

        // FIXME: what if the very first update is reordered?
        m_put_object_index_last_id = id;
        m_put_object_index_queue.pop();
        m_put_object_index_mutex.unlock();

        // update string index
        std::string collection;
        *changes >> collection;
        auto &col = get_collection(collection, true);
        col.primary_index().apply_changes(*changes);

        // invalidate data block cache
        if(block_page_no != INVALID_PAGE_NO)
        {
            auto &shard = *m_shards[shard_id];
            shard.write_lock();
            shard.discard_cached_block(block_page_no);
            shard.write_unlock();
        }

        delete changes;
    }
}

bool Ledger::create_index(const std::string &collection, const std::string &name, const std::vector<std::string> &paths)
{
    auto &col = get_collection(collection, true);
    return col.create_index(name, paths, m_enclave, *this);
}

bool Ledger::drop_index(const std::string &collection, const std::string &name)
{
    auto &col = get_collection(collection);
    return col.drop_index(name);
}

bool Ledger::clear(const OpContext &op_context, const std::string &collection)
{
    auto &col = get_collection(collection);

    std::string pos;
    bool done = false;

    while(!done)
    {
        auto it = col.primary_index().begin_at(pos);

        LockHandle lock_handle(*this);

        // Check a batch and then clean up
        // TODO come up with a generic way to manage long-running tasks
        constexpr size_t BATCH_SIZE = 100;
        size_t c = 0;

        for(; !it.at_end() && c < BATCH_SIZE; ++it)
        {
            const std::string key = it.key();

            auto shard = get_shard(collection, key);
            auto pending = lock_handle.get_pending_block(shard, LockType::Write);

            event_id_t previous_id = it.value();
            ObjectEventHandle previous_event;
            // FIXME: get_event loads a block and doesn't evict it for you, then out of memory
            if(!get_event(previous_event, previous_id, lock_handle, LockType::Write))
            {
                throw std::runtime_error("Broken object reference");
            }

            if(previous_event.get_type() != ObjectEventType::Deletion)
            {
                auto index = put_tombstone(op_context, collection, key, previous_id, lock_handle);
                bitstream changes;
                changes << collection;
                it.set_value({ shard, pending->identifier(), index }, &changes);
                m_object_count -= 1;

                // tell downstream
                send_index_updates_to_downstream(changes, shard, pending->identifier());
            }

            c += 1;
        }

        if(it.at_end())
        {
            done = true;
        }
        else
        {
            pos = it.key();
        }

        it.clear();
        lock_handle.clear(); // don't check evict, since we are going to call organize_ledger for
                             // all shards

        for(uint16_t i = 0; i < NUM_SHARDS; ++i)
        {
            organize_ledger(i);
        }
    }

    return true;
}

void Ledger::organize_ledger(shard_id_t shard_no)
{
    auto &shard = *m_shards[shard_no];
    shard.write_lock();

    auto pending = shard.get_block(shard.pending_block_id());

    // Wait until we have reached at least min block size
    if(pending->byte_size() < MIN_BLOCK_SIZE)
    {
        shard.write_unlock();
        return;
    }
    pending->seal();

    auto newb = shard.generate_block();
    newb->flush_page();

    shard.write_unlock();
}

bool Ledger::get_latest_version(ObjectEventHandle &event,
                                const OpContext &op_context,
                                const std::string &collection,
                                const std::string &key,
                                const std::string &path,
                                event_id_t &id,
                                LockHandle &lock_handle,
                                LockType lock_type,
                                const OperationType access_type)
{
    if(!get_latest_event(event, collection, key, id, lock_handle, lock_type))
    {
        return false;
    }

    while(event.get_type() != ObjectEventType::NewVersion)
    {
        if(event.get_type() == ObjectEventType::Deletion)
        {
            event.clear();
            lock_handle.release_block(id.shard, id.block, lock_type);
            id = INVALID_EVENT;
            return false;
        }

        block_id_t previous_block = id.block;

        id = { id.shard, event.previous_block(), event.previous_index() };
        lock_handle.get_block(id.shard, id.block, lock_type)->get_event(event, id.index);

        lock_handle.release_block(id.shard, previous_block, lock_type);
    }

    json::Document policy("");

    if(!event.get_policy(policy))
    {
        // Object has no security policy
        return true;
    }
    else
    {
        return check_object_policy(policy, op_context, collection, key, path, access_type, lock_handle);
    }
}

bool Ledger::get_latest_event(ObjectEventHandle &hdl,
                              const std::string &collection,
                              const std::string &key,
                              event_id_t &event_id,
                              LockHandle &lock_handle,
                              LockType lock_type)
{
    event_id = INVALID_EVENT;

    if(key.empty())  
    {
        return false;
    }

    auto p_col = try_get_collection(collection);

    if(!p_col)
    {
        return false;
    }

    auto &col = *p_col;

    if(!col.primary_index().get(key, event_id))
    {
        return false;
    }

    auto block = lock_handle.get_block(event_id.shard, event_id.block, lock_type);
    block->get_event(hdl, event_id.index);
    return true;
}

uint32_t Ledger::count_objects(const OpContext &op_context, const std::string &collection, const json::Document &predicates)
{
    if(collection.empty())
    {
        return num_objects();
    }

    auto it = find(op_context, collection, predicates);

    uint32_t count = 0;
    std::string key;
    ObjectEventHandle _;

    while(it.next(key, _))
    {
        count += 1;
    }

    return count;
}

event_id_t
Ledger::remove(const OpContext &op_context, const std::string &collection, const std::string &key, LockHandle *lock_handle_)
{
    if(!Ledger::is_valid_key(key))
    {
        return INVALID_EVENT;
    }

    LockHandle lock_handle(*this, lock_handle_);
    event_id_t previous_id = INVALID_EVENT;
    ObjectEventHandle hdl;

    auto shard = get_shard(collection, key);
    auto &col = get_collection(collection);

    auto pending = lock_handle.get_pending_block(shard, LockType::Write);

    if(!get_latest_event(hdl, collection, key, previous_id, lock_handle, LockType::Write))
    {
        // no such object.
        return INVALID_EVENT;
    }

    if(hdl.get_type() == ObjectEventType::Deletion)
    {
        return INVALID_EVENT;
    }

    auto index = put_tombstone(op_context, collection, key, previous_id, lock_handle);

    event_id_t event_id;
    event_id.shard = shard;
    event_id.block = pending->identifier();
    event_id.index = index;

    col.primary_index().insert(key, event_id);
#ifndef TEST
    col.notify_triggers(m_enclave.remote_parties());
#endif

    m_object_count -= 1;
    lock_handle.clear();

    if(!lock_handle_)
    {
        organize_ledger(shard);
    }

    return event_id;
}

event_index_t Ledger::put_tombstone(const OpContext &op_context,
                                    const std::string &collection,
                                    const std::string &key,
                                    const event_id_t &previous_id,
                                    LockHandle &lock_handle)
{
    if(!op_context.valid())
    {
        throw std::runtime_error("Cannot modify using invalid identity");
    }

    auto shard = get_shard(collection, key);
    auto pending = lock_handle.get_pending_block(shard, LockType::Write);

    json::Writer writer;

    writer.start_array("");
    writer.write_integer("", static_cast<int32_t>(ObjectEventType::Deletion));
    writer.write_string("", op_context.to_string());
    writer.write_integer("", previous_id.block);
    writer.write_integer("", previous_id.index);
    writer.end_array();

    auto doc = writer.make_document();
    auto index = pending->insert(doc);
    pending->flush_page();

    lock_handle.release_block(shard, pending->identifier(), LockType::Write);
    return index;
}

ObjectListIterator Ledger::find(const OpContext &op_context,
                                const std::string &collection,
                                const json::Document &predicates,
                                LockHandle *lock_handle)
{
    auto p_col = try_get_collection(collection);

    if(!p_col)
    {
        return ObjectListIterator(op_context, collection, predicates, *this, lock_handle, nullptr);
    }

    std::set<std::string> paths;
    std::vector<std::pair<size_t, Index *>> indexes;

    auto &col = *p_col;

    for(auto &it : col.secondary_indexes())
    {
        auto index = it.second;
        bool contains = true;
        for(auto &p : index->paths())
        {
            if(paths.find(p) == paths.end())
            {
                contains = false;
            }
        }

        // Already covered by other indexes
        if(contains || !index->matches_query(predicates))
        {
            continue;
        }

        json::Document view(predicates, index->paths());
        if(!view.empty())
        {
            // reminder: keep this piece of code in sync with the code below
            json::Document view(predicates, index->paths());

            if(view.get_size() > 1)
            {
                throw std::runtime_error("Fixme");
            }

            auto size = index->estimate_value_count(view);
            log_debug("estimation: will find " + std::to_string(size) + " keys from index " + index->name());

            indexes.emplace_back(size, index);
            for(auto &p : index->paths())
            {
                paths.insert(p);
            }
        }
    }

    if(!indexes.empty())
    {
        // heuristic: let the smallest result set be the first set in the merged keys
        std::sort(indexes.begin(), indexes.end(),
                  [](const auto &lhs, const auto &rhs) { return lhs.first < rhs.first; });

        bool first = true;
        std::unordered_set<std::string> candidates;

        // set intersection
        for(auto &it : indexes)
        {
            auto index = it.second;

            // Only use the part of the predicate that the index can help us with
            json::Document view(predicates, index->paths());
            
            if(view.get_size() > 1)
            {
                throw std::runtime_error("Fixme");
            }

            index->find(view, candidates, first ? SetOperation::Union : SetOperation::Intersect);
            first = false;
        }

        std::vector<std::string> merged_keys(candidates.begin(), candidates.end());
        log_debug("merged keys size " + std::to_string(merged_keys.size()) + " from " +
                  std::to_string(indexes.size()) + " indexes");
        std::unique_ptr<VectorObjectKeyProvider> key_provider(new VectorObjectKeyProvider(std::move(merged_keys)));
        return ObjectListIterator(op_context, collection, predicates, *this, lock_handle, std::move(key_provider));
    }
    else
    {
        // Do linear scan :(
        log_debug("linear scan");
        std::unique_ptr<StringIndex::LinearScanKeyProvider> key_provider(
        new StringIndex::LinearScanKeyProvider(col.primary_index()));

        return ObjectListIterator(op_context, collection, predicates, *this, lock_handle, std::move(key_provider));
    }
}

ObjectIterator
Ledger::iterate(const OpContext &op_context, const std::string &collection, const std::string &key, LockHandle *lock_handle)
{
    return ObjectIterator(op_context, collection, key, *this, lock_handle);
}

void Ledger::load_upstream_index_root(const std::vector<std::string> &collection_names)
{
    for(auto &name : collection_names)
    {
        log_debug("Reload StringIndex root of collection [" + name + "]");
        get_collection(name, true).primary_index().reload_root_node();
    }
}

void Ledger::dump_metadata(bitstream &output)
{
    // ledger
    output << m_object_count;
    output << m_version_count;

    output << m_collections.size();

    for(auto &it : m_collections)
    {
        output << it.first;
        it.second.dump_metadata(output);
    }

    log_info("Ledger metadata dumped");
}

void Ledger::unload_everything()
{
    for(auto &it : m_collections)
    {
        auto &col = it.second;
        col.unload_everything();
    }

    for(auto &shard : m_shards)
    {
        shard->unload_everything();
    }
}

void Ledger::load_metadata(bitstream &input)
{
    // ledger
    input >> m_object_count;
    input >> m_version_count;

    size_t num_collections = 0;
    input >> num_collections;

    for(size_t i = 0; i < num_collections; ++i)
    {
        std::string name;
        input >> name;

        auto it = m_collections.find(name);

        if(it == m_collections.end())
        {
            m_collections.emplace(name, Collection(m_buffer_manager, name));
            it = m_collections.find(name);
        }

        auto &col = it->second;
        col.load_metadata(input);
    }

    for(auto &shard : m_shards)
    {
        shard->load_metadata(input);
    }

    log_info("Ledger metadata loaded");
}

} // namespace trusted
} // namespace credb
