#include "Database.h"

#include "../Enclave.h"
#include "../Ledger.h"
#include "../Peers.h"
#include "../PendingBitstreamResponse.h"
#include "../PendingBooleanResponse.h"
#include "../ProgramRunner.h"
#include "../logging.h"

#include "Collection.h"
#include "Transaction.h"

using namespace cow;

namespace credb
{
namespace trusted
{
namespace bindings
{

Database::Database(MemoryManager &mem,
                   const OpContext &op_context,
                   credb::trusted::Ledger &ledger,
                   credb::trusted::Enclave &enclave,
                   ProgramRunner *runner,
                   LockHandle &lock_handle)
: Module(mem), m_op_context(op_context), m_ledger(ledger),
  m_lock_handle(lock_handle)
#ifndef TEST
  , m_runner(runner), m_enclave(enclave), m_peers(m_enclave.peers())
#endif
{
#ifdef TEST
    (void)runner;
    (void)enclave;
#endif
}

cow::ValuePtr Database::get_member(const std::string &name)
{
    auto &mem = memory_manager();

#ifndef TEST
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
        return make_value<Function>(mem, [&mem, this](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(!args.empty())
            {
                throw std::runtime_error("Invalid number of arguments");
            }
            
            return cow::make_value<Transaction>(mem, m_op_context, m_ledger, m_lock_handle);
        });
    }
    else if(name == "peers")
    {
        return make_value<Function>(mem, [&](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(!args.empty())
            {
                throw std::runtime_error("Invalid number of arguments");
            }

            auto it = m_peers.iterate();
            std::vector<std::string> peers;
            auto list = mem.create_list();

            while(it.has_next())
            {
                list->append(mem.create_string(it.next().name()));
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

                auto collection = value_cast<StringVal>(args[0])->get();
                auto peer_name = value_cast<StringVal>(args[1])->get();
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

                auto peer = m_peers.find_by_name(peer_name);

                if(!peer)
                {
                    // FIXME implement exception handling...
                    throw std::runtime_error(
                    "Failed to find the peer in the database module of python binding");
                    return m_mem.create_boolean(false);
                }

                peer->lock();
                auto op_id = peer->call(collection, m_runner->identifier(), key, path, pargs);
                peer->unlock();

                bitstream bs;
                PendingBitstreamResponse pending(op_id, *peer);

                while(!pending.has_message())
                {
                    m_runner->suspend();

                    peer->lock();
                    pending.wait(false);
                    peer->unlock();
                }

                pending.move(bs);
                bool success = false;
                bs >> success;

                if(success)
                {
                    return cow::read_value(bs, m_mem);
                }
                
                std::string error_str;
                bs >> error_str;
                throw std::runtime_error("Call failed in the database module of python binding: [" + error_str + "]");
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

} // namespace bindings
} // namespace trusted
} // namespace credb
