#include "Database.h"

#include "../Enclave.h"
#include "../Ledger.h"
#include "../RemoteParties.h"
#include "../PendingCallResponse.h"
#include "../ProgramRunner.h"
#include "../logging.h"

#include "Collection.h"
#include "Transaction.h"

using namespace cow;

namespace credb::trusted::bindings
{

Database::Database(MemoryManager &mem,
                   const OpContext &op_context,
                   credb::trusted::Ledger &ledger,
                   credb::trusted::Enclave &enclave,
                   ProgramRunner *runner,
                   LockHandle &lock_handle)
: Module(mem), m_op_context(op_context), m_ledger(ledger),
  m_lock_handle(lock_handle)
#ifndef IS_TEST
  , m_runner(runner), m_enclave(enclave), m_remote_parties(m_enclave.remote_parties())
#endif
{
#ifdef IS_TEST
    (void)runner;
    (void)enclave;
#endif
}

cow::ValuePtr Database::get_member(const std::string &name)
{
    auto &mem = memory_manager();

#ifndef IS_TEST
    if(name == "name")
    {
        return make_value<Function>(mem, [&](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(!args.empty())
            {
                throw std::runtime_error("Invalid number of arguments");
            }

            return mem.create_string(m_enclave.name());
        });
    }
    else if(name == "init_transaction")
    {
        if(!has_program_runner())
        {
            return nullptr;
        }
        else
        {
            return make_value<Function>(mem, [&mem, this](const std::vector<ValuePtr> &args) -> ValuePtr {
                if(!args.empty())
                {
                    throw std::runtime_error("Invalid number of arguments");
                }
                
                return cow::make_value<Transaction>(mem, m_ledger, m_enclave, *m_runner, m_lock_handle);
            });
        }
    }
    else if(name == "peers")
    {
        return make_value<Function>(mem, [&](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(!args.empty())
            {
                throw std::runtime_error("Invalid number of arguments");
            }

            auto peers = m_remote_parties.get_peer_infos();
            auto list = mem.create_list();

            for(auto &[name, hostname, port]: peers)
            {
                (void)hostname;
                (void)port;
                list->append(mem.create_string(name));
            }

            return list;
        });
    }
    else if(name == "call_on_peer")
    {
        if(!m_runner)
        {
            return nullptr;
        }
        else
        {
            return make_value<Function>(mem, [this](const std::vector<ValuePtr> &args) -> ValuePtr {
                if(args.size() != 4 || args[0]->type() != ValueType::String ||
                   args[1]->type() != ValueType::String || args[2]->type() != ValueType::String ||
                   args[3]->type() != ValueType::List)
                {
                    throw std::runtime_error("Invalid arguments");
                }

                auto peer_name = value_cast<StringVal>(args[0])->get();
                auto collection = value_cast<StringVal>(args[1])->get();
                auto full_path = value_cast<StringVal>(args[2])->get();

                std::vector<std::string> pargs;
                for(auto arg : value_cast<List>(args[3])->elements())
                {
                    auto str = value_cast<StringVal>(arg)->get();
                    pargs.push_back(str);
                }

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

                auto peer = m_remote_parties.find_by_name<Peer>(peer_name);

                if(!peer)
                {
                    // FIXME implement exception handling...
                    throw std::runtime_error("Failed to find peer '" + peer_name + "' in the database module of python binding");
                    return m_mem.create_boolean(false);
                }

                peer->lock();
                auto op_id = peer->call(collection, m_runner->identifier(), key, path, pargs);
                PendingCallResponse pending(op_id, *peer, m_mem);
                peer->unlock();

                while(!pending.has_message())
                {
                    runner().suspend();

                    peer->lock();
                    pending.wait(false);
                    peer->unlock();
                }

                if(pending.success())
                {
                    return pending.return_value();
                }
                else
                {
                    throw std::runtime_error("Call failed in the database module of python binding: [" + pending.error() + "]");
                }
            });
        }
    }
    else
#endif
    if(name == "get_collection")
    {
        return make_value<Function>(mem, [this](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(args.size() != 1 || args[0]->type() != ValueType::String)
            {
                throw std::runtime_error("Invalid number of arguments");
            }

            auto name = value_cast<StringVal>(args[0])->get();
            return cow::make_value<Collection>(m_mem, m_op_context, m_ledger, m_lock_handle, name);
        });
    }
    else
    {
        throw std::runtime_error("No such method Database::" + name);
    }
}

} // namespace credb::trusted::bindings
