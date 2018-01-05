#include "Transaction.h"
#include "Ledger.h"
#include "Witness.h"
#include "OpContext.h"

namespace credb
{
namespace trusted
{

void Transaction::set_read_lock_if_not_present(shard_id_t sid)
{
    if(!shards_lock_type.count(sid))
    {
        shards_lock_type[sid] = LockType::Read;
    }
}

bool Transaction::check_repeatable_read(ObjectEventHandle &obj,
                           const std::string &collection,
                           const std::string &key,
                           shard_id_t sid,
                           const event_id_t &eid)
{
    const LockType lock_type = shards_lock_type[sid];
    event_id_t latest_eid;
    bool res = ledger.get_latest_version(obj, op_context, collection, key, "", latest_eid,
                                             lock_handle, lock_type);

    if(!res || latest_eid != eid)
    {
        error = "Key [" + key + "] reads outdated value";
        return false;
    }
    return true;
}

struct operation_info_t
{
    virtual OperationType type() const = 0;
    virtual void read_from_req(Transaction &ctx) = 0;
    virtual void collect_shard_lock_type(Transaction &ctx) = 0;
    virtual bool validate_read(Transaction &ctx) = 0;
    virtual bool do_write(Transaction &ctx) = 0;

    operation_info_t(operation_info_t &other) = delete;
    operation_info_t() = default;
    virtual ~operation_info_t() = default;
};

struct write_op_t : public operation_info_t
{
    bool validate_read(Transaction &ctx) override
    {
       (void)ctx;
       return true;
    }
};

struct read_op_t : public operation_info_t
{
    bool do_write(Transaction &ctx) override
    {
       (void)ctx;
       return true;
    }
};

struct has_obj_info_t : public read_op_t
{
    std::string collection;
    std::string key;
    bool result;
    shard_id_t sid;

    OperationType type() const override
    {
        return OperationType::HasObject;
    }

    void read_from_req(Transaction &ctx) override
    {
        ctx.req >> collection >> key >> result;
        sid = ctx.ledger.get_shard(collection, key);
    }

    void collect_shard_lock_type(Transaction &ctx) override
    {
        ctx.set_read_lock_if_not_present(sid);
    }

    bool validate_read(Transaction &ctx) override
    {
        ObjectEventHandle obj;

        {
            auto res = ctx.ledger.has_object(collection, key);
            
            if(res != result)
            {
                ctx.error = "Key [" + key + "] reads outdated value";
                return false;
            }
        }

        if(ctx.generate_witness)
        {
            auto &writer = ctx.writer;

            writer.start_map("");
            writer.write_string("type", "HasObject");
            writer.write_string("key", key);
            writer.write_boolean("result", result);
            writer.end_map();
        }

        return true;
    }
};

struct get_info_t : public read_op_t
{
    std::string collection;
    std::string key;
    shard_id_t sid;
    event_id_t eid;

    OperationType type() const override
    {
        return OperationType::GetObject;
    }

    void read_from_req(Transaction &ctx) override
    {
        ctx.req >> collection >> key >> eid;
        sid = ctx.ledger.get_shard(collection, key);
    }

    void collect_shard_lock_type(Transaction &ctx) override
    {
        ctx.set_read_lock_if_not_present(sid);
    }

    bool validate_read(Transaction &ctx) override
    {
        ObjectEventHandle obj;
        if(ctx.isolation != IsolationLevel::ReadCommitted)
        {
            if(!ctx.check_repeatable_read(obj, collection, key, sid, eid))
            {
                return false;
            }
        }
        else
        {
            const LockType lock_type = ctx.shards_lock_type[sid];
            event_id_t latest_eid;
            
            bool res = ctx.ledger.get_latest_version(obj, ctx.op_context, collection, key, "",
                                                     latest_eid, ctx.lock_handle, lock_type);

            if(!res)
            {
                return false;
            }
        }

        if(ctx.generate_witness)
        {
            auto &writer = ctx.writer;

            writer.start_map("");
            writer.write_string("type", "GetObject");
            writer.write_string("key", key);
            writer.write_integer("shard", eid.shard);
            writer.write_integer("block", eid.block);
            writer.write_integer("index", eid.index);
            writer.write_document("content", obj.value());
            writer.end_map();
        }
        return true;
    }
};

struct put_info_t : public write_op_t
{
    std::string collection;
    std::string key;

    shard_id_t sid;
    json::Document doc;

    OperationType type() const override
    {
        return OperationType::PutObject;
    }

    void read_from_req(Transaction &ctx) override
    {
        ctx.req >> collection >> key >> doc;
        sid = ctx.ledger.get_shard(collection, key);
    }

    void collect_shard_lock_type(Transaction &ctx) override
    {
        ctx.shards_lock_type[sid] = LockType::Write;
    }

    bool do_write(Transaction &ctx) override
    {
        const event_id_t new_eid = ctx.ledger.put(ctx.op_context, collection, key, doc, "", &ctx.lock_handle);

        if(new_eid && ctx.generate_witness)
        {
            auto &writer = ctx.writer;

            writer.start_map("");
            writer.write_string("type", "PutObject");
            writer.write_string("collection", collection);
            writer.write_string("key", key);
            writer.write_integer("shard", new_eid.shard);
            writer.write_integer("block", new_eid.block);
            writer.write_integer("index", new_eid.index);
            writer.write_document("content", doc);
            writer.end_map();
        }
        return true;
    }
};

struct add_info_t : public write_op_t
{
    std::string collection;
    std::string key;
    shard_id_t sid;
    json::Document doc;

    OperationType type() const override
    {
        return OperationType::AddToObject;
    }

    void read_from_req(Transaction &ctx) override
    {
        ctx.req >> collection >> key >> doc;
        sid = ctx.ledger.get_shard(collection, key);
    }

    void collect_shard_lock_type(Transaction &ctx) override
    {
        ctx.shards_lock_type[sid] = LockType::Write;
    }

    bool do_write(Transaction &ctx) override
    {
        const event_id_t new_eid = ctx.ledger.add(ctx.op_context, collection, key, doc, "", &ctx.lock_handle);
        if(ctx.generate_witness)
        {
            auto &writer = ctx.writer;

            writer.start_map("");
            writer.write_string("type", "AddObject");
            writer.write_string("key", key);
            writer.write_integer("shard", new_eid.shard);
            writer.write_integer("block", new_eid.block);
            writer.write_integer("index", new_eid.index);
            writer.write_document("content", doc);
            writer.end_map();
        }
        return true;
    }
};

struct remove_info_t : public write_op_t
{
    std::string collection;
    std::string key;

    shard_id_t sid;

    OperationType type() const override
    {
        return OperationType::RemoveObject;
    }

    void read_from_req(Transaction &ctx) override
    {
        ctx.req >> collection >> key;
        sid = ctx.ledger.get_shard(collection, key);
    }

    void collect_shard_lock_type(Transaction &ctx) override
    {
        ctx.shards_lock_type[sid] = LockType::Write;
    }

    bool do_write(Transaction &ctx) override
    {
        const event_id_t new_eid = ctx.ledger.remove(ctx.op_context, collection, key, &ctx.lock_handle);
        if(ctx.generate_witness)
        {
            auto &writer = ctx.writer;

            writer.start_map("");
            writer.write_string("type", "RemoveObject");
            writer.write_string("key", key);
            writer.write_integer("shard", new_eid.shard);
            writer.write_integer("block", new_eid.block);
            writer.write_integer("index", new_eid.index);
            writer.end_map();
        }
        return true;
    }
};

struct find_info_t : public read_op_t
{
    std::string collection;
    json::Document predicates;
    std::vector<std::string> projection;
    std::vector<std::tuple<std::string, shard_id_t, event_id_t>> res;
    int32_t limit;

    OperationType type() const override
    {
        return OperationType::FindObjects;
    }

    void read_from_req(Transaction &ctx) override
    {
        ctx.req >> collection >> predicates >> projection >> limit;
        uint32_t size = 0;
        ctx.req >> size;
        for(uint32_t i = 0; i < size; ++i)
        {
            std::string key;
            event_id_t eid;
            ctx.req >> key >> eid;
            shard_id_t sid = ctx.ledger.get_shard(collection, key);
            res.emplace_back(key, sid, eid);
        }
    }

    void collect_shard_lock_type(Transaction &ctx) override
    {
        for(const auto &it : res)
        {
            ctx.set_read_lock_if_not_present(std::get<1>(it));
        }
    }

    void write_witness(Transaction &ctx,
                       const std::string &key,
                       const event_id_t &eid,
                       const json::Document &value)
    {
        auto &writer = ctx.writer;

        writer.start_map("");
        writer.write_string("key", key);
        writer.write_integer("shard", eid.shard);
        writer.write_integer("block", eid.block);
        writer.write_integer("index", eid.index);
        writer.write_document("content", value);
        writer.end_map();
    }

    bool validate_no_dirty_read(Transaction &ctx)
    {
        for(const auto & [key, sid, eid] : res)
        {
            const LockType lock_type = ctx.shards_lock_type[sid];
            event_id_t latest_eid;
            ObjectEventHandle obj;
            bool res = ctx.ledger.get_latest_version(obj, ctx.op_context, collection, key, "",
                                                     latest_eid, ctx.lock_handle, lock_type);
            if(!res)
            {
                ctx.error = "Key [" + key + "] reads outdated value";
                return false;
            }

            if(ctx.generate_witness)
            {
                write_witness(ctx, key, eid, obj.value());
            }
        }
        return true;
    }

    bool validate_repeatable_read(Transaction &ctx)
    {
        for(const auto & [key, sid, eid] : res)
        {
            ObjectEventHandle obj;
            if(!ctx.check_repeatable_read(obj, collection, key, sid, eid))
            {
                return false;
            }

            if(ctx.generate_witness)
            {
                write_witness(ctx, key, eid, obj.value());
            }
        }
        return true;
    }

    bool validate_no_phantom(Transaction &ctx)
    {
        // build the set of known result
        std::unordered_set<event_id_t> eids;
        for(const auto &it : res)
        {
            eids.emplace(std::get<2>(it));
        }

        // find again and check if there is phantom read
        auto it = ctx.ledger.find(ctx.op_context, collection, predicates, &ctx.lock_handle);
        std::string key;
        ObjectEventHandle hdl;
        for(auto eid = it.next(key, hdl); hdl.valid(); eid = it.next(key, hdl))
        {
            auto cnt = eids.erase(eid);
            if(!cnt)
            {
                ctx.error = "Phantom read: key=" + key;
                return false;
            }
            if(ctx.generate_witness)
            {
                json::Document value = hdl.value();
                if(!projection.empty())
                {
                    json::Document filtered(value, projection);
                    write_witness(ctx, key, eid, filtered);
                }
                else
                {
                    write_witness(ctx, key, eid, value);
                }
            }
        }
        if(!eids.empty())
        {
            ctx.error = "Phantom read: too few results";
            return false;
        }

        return true;
    }

    bool validate_read(Transaction &ctx) override
    {
        if(ctx.generate_witness)
        {
            auto &writer = ctx.writer;

            writer.start_map("");
            writer.write_string("type", "FindObjects");
            writer.write_string("collection", collection);
            writer.write_document("predicates", predicates);
            writer.start_array("projection");
            for(const auto &proj : projection)
            {
                writer.write_string("", proj);
            }
            writer.end_array();
            writer.write_integer("limit", limit);
            writer.start_array("results");
        }

        bool ok = false;
        switch(ctx.isolation)
        {
        case IsolationLevel::ReadCommitted:
            ok = validate_no_dirty_read(ctx);
            break;
        case IsolationLevel::RepeatableRead:
            ok = validate_repeatable_read(ctx);
            break;
        case IsolationLevel::Serializable:
            ok = validate_no_phantom(ctx);
            break;
        default:
            ctx.error = "Unknown IsolationLevel " + std::to_string(static_cast<uint8_t>(ctx.isolation));
            return false;
        }

        if(!ok)
        {
            return false;
        }

        if(ctx.generate_witness)
        {
            ctx.writer.end_array();
            ctx.writer.end_map();
        }
        return true;
    }
};

operation_info_t *new_operation_info_from_req(Transaction &ctx)
{
    OperationType op;
    ctx.req >> reinterpret_cast<uint8_t &>(op);
    operation_info_t *p = nullptr;
    
    switch(op)
    {
    case OperationType::GetObject:
        p = new get_info_t;
        break;
    case OperationType::HasObject:
        p = new has_obj_info_t;
        break;
    case OperationType::PutObject:
        p = new put_info_t;
        break;
    case OperationType::AddToObject:
        p = new add_info_t;
        break;
    case OperationType::FindObjects:
        p = new find_info_t;
        break;
    case OperationType::RemoveObject:
        p = new remove_info_t;
        break;
    default:
        ctx.error = "Unknown OperationType " + std::to_string(static_cast<uint8_t>(op));
        log_error(ctx.error);
        return nullptr;
    }
    p->read_from_req(ctx);
    return p;
}

void Transaction::commit()
{
    req >> reinterpret_cast<uint8_t &>(isolation);
    req >> generate_witness;

    if(isolation == IsolationLevel::Serializable)
    {
        // FIXME: how to avoid phantom read while avoiding locking all shards?
        for(shard_id_t i = 0; i < NUM_SHARDS; ++i)
        {
            shards_lock_type[i] = LockType::Read;
        }
    }

    // construct op history and also collect shards_lock_type
    uint32_t ops_size = 0;
    req >> ops_size;
    for(uint32_t i = 0; i < ops_size; ++i)
    {
        auto op = new_operation_info_from_req(*this);
        if(!op)
        {
            return;
        }

        ops.push_back(op);

        op->collect_shard_lock_type(*this);
    }

    // first acquire locks for all pending shards to ensure atomicity
    for(auto &kv : shards_lock_type)
    {
        lock_handle.get_shard(kv.first, kv.second);
    }

    // witness root
    if(generate_witness)
    {
        writer.start_map("");
        // TODO: timestamp
        switch(isolation)
        {
        case IsolationLevel::ReadCommitted:
            writer.write_string("isolation", "ReadCommitted");
            break;
        case IsolationLevel::RepeatableRead:
            writer.write_string("isolation", "RepeatableRead");
            break;
        case IsolationLevel::Serializable:
            writer.write_string("isolation", "Serializable");
            break;
        }
        writer.start_array(Witness::OP_FIELD_NAME);
    }

    // validate reads
    for(auto op : ops)
    {
        if(!op->validate_read(*this))
        {
            assert(!error.empty());
            return;
        }
    }

    // then do writes
    for(auto op : ops)
    {
        if(!op->do_write(*this))
        {
            assert(!error.empty());
            return;
        }
    }

    // close witness root
    if(generate_witness)
    {
        writer.end_array(); // operations
        writer.end_map(); // witness root
    }
}

Transaction::~Transaction()
{
    // release previously locked shards
    for(auto &kv : shards_lock_type)
    {
        lock_handle.release_shard(kv.first, kv.second);
    }

    // check evict
    lock_handle.clear();
    for(auto &kv : shards_lock_type)
    {
        ledger.organize_ledger(kv.first);
    }
}

void Transaction::get_output(bitstream &output)
{
    if(error.empty())
    {
        // create witness
        if(generate_witness)
        {
            json::Document doc = writer.make_document();
            witness.set_data(std::move(doc.data()));
            if(!sign_witness(ledger.m_enclave, witness))
            {
                error = "cannot sign witness";
            }
        }
    }

    bitstream bstream;
    if(error.empty())
    {
        bstream << true << witness;
    }
    else
    {
        bstream << false << error;
    }
    
    output << bstream;
}

}
}
