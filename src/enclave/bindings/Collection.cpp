#include "Collection.h"

#include "../Enclave.h"
#include "../Ledger.h"
#include "../ProgramRunner.h"
#include "../logging.h"

#include "ObjectIterator.h"
#include <cowlang/cow.h>
#include <cowlang/unpack.h>

using namespace cow;

namespace credb
{
namespace trusted
{
namespace bindings
{

Collection::Collection(MemoryManager &mem,
                       const OpContext &op_context,
                       credb::trusted::Ledger &ledger,
                       LockHandle &lock_handle,
                       const std::string &name)
: Module(mem), m_op_context(op_context), m_ledger(ledger), m_lock_handle(lock_handle), m_name(name)
{
}

cow::ValuePtr Collection::get_member(const std::string &name)
{
    auto &mem = memory_manager();

    if(name == "count_writes")
    {
        return make_value<Function>(mem, [&](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(args.size() != 2 || args[0]->type() != ValueType::String || args[1]->type() != ValueType::String)
            {
                throw std::runtime_error("Invalid arguments");
            }

            auto str1 = value_cast<StringVal>(args[0]);
            auto str2 = value_cast<StringVal>(args[1]);

            return mem.create_integer(
            m_ledger.count_writes(m_op_context, m_name, str1->get(), str2->get(), &m_lock_handle));
        });
    }
    else if(name == "get")
    {
        return make_value<Function>(mem, [&](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(args.size() != 1 || args[0]->type() != ValueType::String)
            {
                throw std::runtime_error("Invalid arguments");
            }

            auto &full_path = value_cast<StringVal>(args[0])->get();
            std::string key, path;
            size_t ppos;

            if((ppos = full_path.find(".")) != std::string::npos)
            {
                path = full_path.substr(ppos + 1, std::string::npos);
                key = full_path.substr(0, ppos);
            }
            else
            {
                key = full_path;
            }

            auto it = m_ledger.iterate(m_op_context, m_name, key, &m_lock_handle);

            ValuePtr res = nullptr;

            ObjectEventHandle hdl;
            if(!it.next(hdl))
            {
                return nullptr;
            }

            if(!path.empty())
            {
                try
                {
                    json::Document view = hdl.value();
                    json::Document view2(view, path);
                    res = mem.create_from_document(view2);
                }
                catch(std::runtime_error)
                {
                }
            }
            else
            {
                json::Document view = hdl.value();
                res = mem.create_from_document(view);
            }

            it.clear();
            return res;
        });
    }
    else if(name == "has_object")
    {
        return make_value<Function>(mem, [&](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(args.size() != 1 && args[0]->type() != ValueType::String)
            {
                throw std::runtime_error("Invalid number of arguments");
            }

            auto key = unpack_string(args[0]);
            auto res = m_ledger.has_object(m_name, key);

            return mem.create_boolean(res);
        });
    }
    else if(name == "find")
    {
        return make_value<Function>(mem, [&](const std::vector<ValuePtr> &args) -> ValuePtr {
            // FIXME support passing predicates
            if(!args.empty())
            {
                throw std::runtime_error("Invalid number of arguments");
            }

            json::Document predicates("");

            auto it = m_ledger.find(m_op_context, m_name, predicates, &m_lock_handle);

            return cow::make_value<ObjectListIterator>(mem, std::move(it));
        });
    }
    else if(name == "put" || name == "add")
    {
        return make_value<Function>(mem, [name, &mem, this](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(args.size() != 2 || args[0]->type() != ValueType::String)
            {
                throw std::runtime_error("Invalid number of arguments");
            }

            auto full_path = value_cast<StringVal>(args[0])->get();
            bool res = true;

            // TODO this is duplicated code from client
            std::string path, key;
            size_t ppos;

            if((ppos = full_path.find(".")) != std::string::npos)
            {
                path = full_path.substr(ppos + 1, std::string::npos);
                key = full_path.substr(0, ppos);

                if(!m_ledger.has_object(m_name, key))
                {
                    log_debug("Cannot update object. Does not exist.");
                }
            }
            else
            {
                key = full_path;
                path = "";
            }

            if(!m_ledger.is_valid_key(key))
            {
                // invalid key;
                log_debug("Cannot put object '" + key + "'. Invalid keyword.");
            }

            if(!res)
            {
                return mem.create_boolean(false);
            }

            auto doc = cow::value_to_document(args[1]);
            event_id_t eid = INVALID_EVENT;

            if(name == "put")
            {
                eid = m_ledger.put(m_op_context, m_name, key, doc, path, &m_lock_handle);
            }
            else
            {
                eid = m_ledger.add(m_op_context, m_name, key, doc, path, &m_lock_handle);
            }

            res = static_cast<bool>(eid);
            return mem.create_boolean(res);
        });
    }
    else
    {
        throw std::runtime_error("failed to get member");
    }
}

} // namespace bindings
} // namespace trusted
} // namespace credb
