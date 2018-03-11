#include "op_info.h"

#include "Transaction.h"
#include "Ledger.h"
#include "util/keys.h"

namespace credb
{
namespace trusted
{

shard_id_t operation_info_t::get_shard(const std::string &collection, const std::string &full_path) const
{
    auto [key, path] = parse_path(full_path);
    (void)path; //path is irrelevant for shard id

    return m_transaction.ledger.get_shard(collection, key);
}

check_obj_info_t::check_obj_info_t(Transaction &tx, bitstream &req)
    : read_op_t(tx)
{
    req >> m_collection >> m_key >> m_predicates >> m_result;
    m_sid = get_shard(m_collection, m_key);
}

check_obj_info_t::check_obj_info_t(Transaction &tx, const std::string &collection, const std::string &key, const json::Document &predicates, bool result)
    : read_op_t(tx), m_collection(collection), m_key(key), m_predicates(predicates.duplicate()), m_result(result),
      m_sid(get_shard(m_collection, m_key))
{}

void check_obj_info_t::collect_shard_lock_type()
{
    transaction().set_read_lock(m_sid);
}

void check_obj_info_t::extract_reads(std::unordered_set<event_id_t> &read_set)
{
    //FIXME
    (void)read_set;
}

bool check_obj_info_t::validate()
{
    {
        auto res = transaction().ledger.check(transaction().op_context, m_collection, m_key, "", m_predicates);
        
        if(res != m_result)
        {
            transaction().error = "Key [" + m_key + "] reads outdated value";
            return false;
        }
    }

    if(transaction().generate_witness)
    {
        auto &writer = transaction().writer;

        writer.start_map("");
        writer.write_string("type", "HasObject");
        writer.write_string("key", m_key);
        writer.write_boolean("result", m_result);
        writer.end_map();
    }

    return true;
}

has_obj_info_t::has_obj_info_t(Transaction &tx, bitstream &req)
    : read_op_t(tx)
{
    req >> m_collection >> m_key >> m_result;
    m_sid = get_shard(m_collection, m_key);
}

has_obj_info_t::has_obj_info_t(Transaction &tx, const std::string &collection, const std::string &key, bool result)
    : read_op_t(tx), m_collection(collection), m_key(key), m_result(result),
      m_sid(get_shard(m_collection, m_key))
{}

void has_obj_info_t::extract_reads(std::unordered_set<event_id_t> &read_set)
{
    //FIXME
    (void)read_set;
}

void has_obj_info_t::collect_shard_lock_type()
{
    transaction().set_read_lock(m_sid);
}

bool has_obj_info_t::validate()
{
    {
        auto res = transaction().ledger.has_object(m_collection, m_key, &transaction().lock_handle);
        
        if(res != m_result)
        {
            transaction().error = "Key [" + m_key + "] reads outdated value";
            return false;
        }
    }

    if(transaction().generate_witness)
    {
        auto &writer = transaction().writer;

        writer.start_map("");
        writer.write_string("type", "HasObject");
        writer.write_string("key", m_key);
        writer.write_boolean("result", m_result);
        writer.end_map();
    }

    return true;
}

get_info_t::get_info_t(Transaction &tx, bitstream &req)
        : read_op_t(tx)
{
    req >> m_collection >> m_key >> m_eid;
    m_sid = get_shard(m_collection, m_key);
}

get_info_t::get_info_t(Transaction &tx, const std::string &collection, const std::string &key, const event_id_t eid)
        : read_op_t(tx), m_collection(collection), m_key(key), m_eid(eid),
           m_sid(get_shard(collection, key))
{}

void get_info_t::extract_reads(std::unordered_set<event_id_t> &read_set)
{
    read_set.insert(m_eid);
}

void get_info_t::collect_shard_lock_type()
{
    transaction().set_read_lock(m_sid);
}

bool get_info_t::validate()
{
    ObjectEventHandle obj;
    if(transaction().isolation_level() != IsolationLevel::ReadCommitted)
    {
        if(!transaction().check_repeatable_read(obj, m_collection, m_key, m_sid, m_eid))
        {
            return false;
        }
    }
    else
    {
        event_id_t latest_eid;
       
        obj = transaction().ledger.get_latest_version(transaction().op_context, m_collection, m_key, "", latest_eid, transaction().lock_handle, LockType::Read);

        if(!obj.valid())
        {
            return false;
        }
    }

    if(transaction().generate_witness)
    {
        auto &writer = transaction().writer;

        writer.start_map();
        writer.write_string("type", "GetObject");
        writer.write_string("key", m_key);
        writer.write_integer("shard", m_eid.shard);
        writer.write_integer("block", m_eid.block);
        writer.write_integer("index", m_eid.index);
        writer.write_document("content", obj.value());
        writer.end_map();
    }
    return true;
}

put_info_t::put_info_t(Transaction &tx, bitstream &req)
    : write_op_t(tx)
{
    req >> m_collection >> m_key >> m_doc;
    m_sid = get_shard(m_collection, m_key);
}

put_info_t::put_info_t(Transaction &tx, const std::string &collection, const std::string &key, const json::Document &doc)
    : write_op_t(tx), m_collection(collection), m_key(key), m_doc(doc.duplicate())
{
    m_sid = get_shard(m_collection, m_key);
}

void put_info_t::extract_writes(std::array<uint16_t, NUM_SHARDS> &write_set)
{
    write_set[m_sid] += 1;
}

void put_info_t::collect_shard_lock_type()
{
    transaction().set_write_lock(m_sid);
}

bool put_info_t::validate()
{
    auto [key, path] = parse_path(m_key);

    return transaction().ledger.prepare_write(transaction().op_context, m_collection, key, path, OperationType::PutObject, &transaction().lock_handle);
}

bool put_info_t::do_write(std::unordered_set<event_id_t> &read_set,
                          std::unordered_set<event_id_t> &write_set)
{
    auto [key, path] = parse_path(m_key);

    const event_id_t new_eid = transaction().ledger.apply_write(transaction().op_context, m_collection, key, m_doc, path, &transaction().lock_handle, OperationType::PutObject, read_set, write_set);

    if(new_eid && transaction().generate_witness)
    {
        auto &writer = transaction().writer;

        writer.start_map();
        writer.write_string("type", "PutObject");
        writer.write_string("collection", m_collection);
        writer.write_string("key", m_key);
        writer.write_integer("shard", new_eid.shard);
        writer.write_integer("block", new_eid.block);
        writer.write_integer("index", new_eid.index);
        writer.write_document("content", m_doc);
        writer.end_map();
    }

    return true;
}

add_info_t::add_info_t(Transaction &tx, bitstream &req)
    : write_op_t(tx)
{
    req >> m_collection >> m_key >> m_doc;
    m_sid = get_shard(m_collection, m_key);
}

add_info_t::add_info_t(Transaction &tx, const std::string &collection, const std::string &key, const json::Document &doc)
    : write_op_t(tx), m_collection(collection), m_key(key), m_doc(doc.duplicate())
{
    m_sid = transaction().ledger.get_shard(m_collection, m_key);
}

bool add_info_t::validate()
{
    auto [key, path] = parse_path(m_key);

    return transaction().ledger.prepare_write(transaction().op_context, m_collection, key, path, OperationType::AddToObject, &transaction().lock_handle);
}

void add_info_t::extract_writes(std::array<uint16_t, NUM_SHARDS> &write_set)
{
    write_set[m_sid] += 1;
}

void add_info_t::collect_shard_lock_type()
{
    transaction().set_write_lock(m_sid);
}

bool add_info_t::do_write(std::unordered_set<event_id_t> &read_set,
                          std::unordered_set<event_id_t> &write_set)
{
    auto [key, path] = parse_path(m_key);
    
    const event_id_t new_eid = transaction().ledger.apply_write(transaction().op_context, m_collection, key, m_doc, path, &transaction().lock_handle, OperationType::AddToObject, read_set, write_set);

    if(transaction().generate_witness)
    {
        auto &writer = transaction().writer;

        writer.start_map("");
        writer.write_string("type", "AddObject");
        writer.write_string("key", m_key);
        writer.write_integer("shard", new_eid.shard);
        writer.write_integer("block", new_eid.block);
        writer.write_integer("index", new_eid.index);
        writer.write_document("content", m_doc);
        writer.end_map();
    }

    return true;
}

remove_info_t::remove_info_t(Transaction &tx, bitstream &req)
    : write_op_t(tx)
{
    req >> m_collection >> m_key;
    m_sid = transaction().ledger.get_shard(m_collection, m_key);
}

bool remove_info_t::validate()
{
    return transaction().ledger.prepare_write(transaction().op_context, m_collection, m_key, "", OperationType::RemoveObject, &transaction().lock_handle);
}

void remove_info_t::extract_writes(std::array<uint16_t, NUM_SHARDS> &write_set)
{
    write_set[m_sid] += 1;
}

void remove_info_t::collect_shard_lock_type()
{
    transaction().set_write_lock(m_sid);
}

bool remove_info_t::do_write(std::unordered_set<event_id_t> &read_set,
                          std::unordered_set<event_id_t> &write_set)
{
    json::Document doc;

    const event_id_t new_eid = transaction().ledger.apply_write(transaction().op_context, m_collection, m_key, doc, "", &transaction().lock_handle, OperationType::RemoveObject, read_set, write_set);

    if(transaction().generate_witness)
    {
        auto &writer = transaction().writer;

        writer.start_map("");
        writer.write_string("type", "RemoveObject");
        writer.write_string("key", m_key);
        writer.write_integer("shard", new_eid.shard);
        writer.write_integer("block", new_eid.block);
        writer.write_integer("index", new_eid.index);
        writer.end_map();
    }
    return true;
}

find_info_t::find_info_t(Transaction &tx, bitstream &req)
        : read_op_t(tx)
{
    req >> collection >> predicates >> projection >> limit;
    uint32_t size = 0;
    req >> size;
    for(uint32_t i = 0; i < size; ++i)
    {
        std::string key;
        event_id_t eid;
        req >> key >> eid;
        shard_id_t sid = transaction().ledger.get_shard(collection, key);
        res.emplace_back(key, sid, eid);
    }
}

void find_info_t::extract_reads(std::unordered_set<event_id_t> &read_set)
{
    for(auto &[key, shard, eid]: res)
    {
        (void)key;
        (void)shard;
        read_set.insert(eid);
    }
}

void find_info_t::collect_shard_lock_type()
{
    if(transaction().isolation_level() == IsolationLevel::Serializable)
    {
        // Lock all shards to avoid phantom reads
        for(shard_id_t i = 0; i < NUM_SHARDS; ++i)
        {
            transaction().set_read_lock(i);
        }
    }
    else
    {
        // Only lock what we read
        for(const auto &it : res)
        {
            transaction().set_read_lock(std::get<1>(it));
        }
    }
}

void find_info_t::write_witness(
                   const std::string &key,
                   const event_id_t &eid,
                   const json::Document &value)
{
    auto &writer = transaction().writer;

    writer.start_map("");
    writer.write_string("key", key);
    writer.write_integer("shard", eid.shard);
    writer.write_integer("block", eid.block);
    writer.write_integer("index", eid.index);
    writer.write_document("content", value);
    writer.end_map();
}

bool find_info_t::validate_no_dirty_read()
{
    for(const auto &[key, sid, eid] : res)
    {
        (void)sid;
        event_id_t latest_eid;

        auto hdl = transaction().ledger.get_latest_version(transaction().op_context, collection, key, "", latest_eid, transaction().lock_handle, LockType::Read);
        
        if(!hdl.valid())
        {
            transaction().error = "Key [" + key + "] reads outdated value";
            return false;
        }

        if(transaction().generate_witness)
        {
            write_witness(key, eid, hdl.value());
        }
    }
    return true;
}

bool find_info_t::validate_repeatable_read()
{
    for(const auto & [key, sid, eid] : res)
    {
        ObjectEventHandle obj;
        if(!transaction().check_repeatable_read(obj, collection, key, sid, eid))
        {
            return false;
        }

        if(transaction().generate_witness)
        {
            write_witness(key, eid, obj.value());
        }
    }
    return true;
}

bool find_info_t::validate_no_phantom()
{
    // build the set of known result
    std::unordered_set<event_id_t> eids;
    for(const auto &it : res)
    {
        eids.emplace(std::get<2>(it));
    }

    // find again and check if there is phantom read
    auto it = transaction().ledger.find(transaction().op_context, collection, predicates, &transaction().lock_handle);
    std::string key;
    ObjectEventHandle hdl;

    for(auto eid = it.next(key, hdl); hdl.valid(); eid = it.next(key, hdl))
    {
        auto cnt = eids.erase(eid);
        if(!cnt)
        {
            transaction().error = "Phantom read: key=" + key;
            return false;
        }

        if(transaction().generate_witness)
        {
            json::Document value = hdl.value();
            if(!projection.empty())
            {
                json::Document filtered(value, projection);
                write_witness(key, eid, filtered);
            }
            else
            {
                write_witness(key, eid, value);
            }
        }
    }

    if(!eids.empty())
    {
        transaction().error = "Phantom read: too few results";
        return false;
    }

    return true;
}

bool find_info_t::validate()
{
    if(transaction().generate_witness)
    {
        auto &writer = transaction().writer;

        writer.start_map();
        writer.write_string("type", "FindObjects");
        writer.write_string("collection", collection);
        writer.write_document("predicates", predicates);
        writer.start_array("projection");
        for(const auto &proj : projection)
        {
            writer.write_string(proj);
        }
        writer.end_array();
        writer.write_integer("limit", limit);
        writer.start_array("results");
    }

    bool ok = false;

    switch(transaction().isolation_level())
    {
    case IsolationLevel::ReadCommitted:
        ok = validate_no_dirty_read();
        break;
    case IsolationLevel::RepeatableRead:
        ok = validate_repeatable_read();
        break;
    case IsolationLevel::Serializable:
        ok = validate_no_phantom();
        break;
    default:
        transaction().error = "Unknown IsolationLevel " + std::to_string(static_cast<uint8_t>(transaction().isolation_level()));
        return false;
    }

    if(!ok)
    {
        return false;
    }

    if(transaction().generate_witness)
    {
        transaction().writer.end_array();
        transaction().writer.end_map();
    }

    return true;
}

}
}
