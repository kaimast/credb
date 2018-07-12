#include "TransactionCollection.h"

#include "../Enclave.h"
#include "../Ledger.h"
#include "../ProgramRunner.h"
#include "../logging.h"
#include "../op_info.h"
#include "../Transaction.h"

#include "util/keys.h"
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

TransactionCollection::TransactionCollection(MemoryManager &mem,
                           const ProgramRunner &runner,
                           credb::trusted::TransactionPtr transaction,
                           credb::trusted::Ledger &ledger,
                           LockHandle &lock_handle,
                           const std::string &name)
    : Module(mem), m_runner(runner), m_ledger(ledger), m_transaction(transaction), m_lock_handle(lock_handle), m_name(name)
{
}

cow::ValuePtr TransactionCollection::get_member(const std::string &name)
{
    auto &mem = memory_manager();

    if(name == "get")
    {
        return make_value<Function>(mem, [&](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(args.size() != 1 || args[0]->type() != ValueType::String)
            {
                throw std::runtime_error("Invalid arguments");
            }

            auto &full_path = value_cast<StringVal>(args[0])->get();
            auto [key, path] = parse_path(full_path);
            
            auto it = m_ledger.iterate(m_runner.op_context(), m_name, key, path, &m_lock_handle);

            auto [eid, value] = it.next();
            
            if(!eid)
            {
                return nullptr;
            }

            auto res = mem.create_from_document(value);

            auto op = new get_info_t(*m_transaction, m_name, full_path, eid, m_runner.identifier());

            m_transaction->register_operation(op);

            return res;
        });
    }
    else if(name == "check")
    {
        return make_value<Function>(mem, [&](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(args.size() != 2 && args[0]->type() != ValueType::String)
            {
                throw std::runtime_error("Invalid number of arguments");
            }

            auto key = unpack_string(args[0]);
            auto predicates = cow::value_to_document(args[1]);

            auto res = m_ledger.check(m_runner.op_context(), m_name, key, "", predicates);

            auto op = new check_obj_info_t(*m_transaction, m_name, key, predicates, res, m_runner.identifier());
            m_transaction->register_operation(op);

            return mem.create_boolean(res);
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
            auto res = m_ledger.has_object(m_name, key, &m_lock_handle);

            auto op = new has_obj_info_t(*m_transaction, m_name, key, res, m_runner.identifier());
            m_transaction->register_operation(op);

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

            auto it = m_ledger.find(m_runner.op_context(), m_name, predicates, &m_lock_handle);

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

            if(name == "put")
            {
                auto op = new put_info_t(*m_transaction, m_name, full_path, doc, m_runner.identifier());
                m_transaction->register_operation(op);
            }
            else
            {
                auto op = new add_info_t(*m_transaction, m_name, full_path, doc, m_runner.identifier());
                m_transaction->register_operation(op);
            }

            return mem.create_boolean(true);
        });
    }
    else
    {
        throw std::runtime_error("No such method TransactionCollection::" + name);
    }
}

} // namespace bindings
} // namespace trusted
} // namespace credb
