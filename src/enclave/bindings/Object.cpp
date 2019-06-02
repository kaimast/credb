#include "Object.h"
#include "../Enclave.h"
#include "../Ledger.h"
#include "../OpContext.h"

#include <cowlang/cow.h>

using namespace cow;

namespace credb::trusted::bindings
{

Object::Object(MemoryManager &mem,
               const credb::trusted::OpContext &op_context,
               Ledger &ledger,
               std::string collection,
               std::string key,
               LockHandle &lock_handle)
: Module(mem), m_op_context(op_context), m_ledger(ledger), m_collection(std::move(collection)), m_key(std::move(key)),
  m_lock_handle(lock_handle)
{
}

ValuePtr Object::get_member(const std::string &name)
{
    auto &mem = memory_manager();

    if(name == "count_writes")
    {
        return make_value<Function>(mem, [&, this](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(args.size() != 1 || args[0]->type() != ValueType::String)
            {
                throw std::runtime_error("Invalid arguments");
            }

            return mem.create_integer(m_ledger.count_writes(m_op_context,
                                                            value_cast<StringVal>(args[0])->get(),
                                                            m_collection, m_key, &m_lock_handle));
        });
    }
    else if(name == "get")
    {
        return make_value<Function>(mem, [&mem, this](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(args.size() != 1 || args[0]->type() != ValueType::String)
            {
                throw std::runtime_error("Invalid arguments");
            }

            auto path = value_cast<StringVal>(args[0])->get();
            auto it = m_ledger.iterate(m_op_context, m_collection, m_key, path, &m_lock_handle);

            auto [eid, value] = it.next();

            if(!eid)
            {
                throw std::runtime_error("self does not exist!");
            }

            return mem.create_from_document(value);
        });
    }
    else if(name == "contains")
    {
        return make_value<Function>(mem, [&mem, this](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(args.size() != 2 || args[0]->type() != ValueType::String || args[1]->type() != ValueType::String)
            {
                throw std::runtime_error("Invalid arguments");
            }

            auto path = value_cast<StringVal>(args[0])->get();
            auto val  = value_cast<StringVal>(args[1])->get(); //TODO allow generic values not only strings

            auto it = m_ledger.iterate(m_op_context, m_collection, m_key, path, &m_lock_handle);

            auto [eid, field] = it.next();
            bool res = false;

            if(eid)
            {
                try 
                {
                    json::Document view(field, val, true);
                    res = true;
                }
                catch(std::exception &e)
                {
                }
            }

            return mem.create_boolean(res);
        });
    }
    else if(name == "put" || name == "add")
    {
        return make_value<Function>(mem, [name, &mem, this](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(args.size() != 2 || args[0]->type() != ValueType::String)
            {
                throw std::runtime_error("Invalid arguments");
            }

            auto path = value_cast<StringVal>(args[0])->get();
            auto doc = cow::value_to_document(args[1]);

            event_id_t eid = INVALID_EVENT;

            if(name == "put")
            {
                eid = m_ledger.put(m_op_context, m_collection, m_key, doc, path, &m_lock_handle);
            }
            else
            {
                eid = m_ledger.add(m_op_context, m_collection, m_key, doc, path, &m_lock_handle);
            }

            return mem.create_boolean(static_cast<bool>(eid));
        });
    }
    else
    {
        throw std::runtime_error("Unknown function: self." + name);
    }
}

} // namespace credb::trusted::bindings
