#include "Transaction.h"

#include "../LockHandle.h"
#include "../Transaction.h"
#include "../RemotePartyRunner.h"
#include "../Peers.h"
#include "../PendingCallResponse.h"
#include "../PendingBitstreamResponse.h"
#include "../logging.h"
#include "TransactionCollection.h"

#include <cowlang/unpack.h>

using namespace cow;

namespace credb
{
namespace trusted
{
namespace bindings
{

Transaction::Transaction(cow::MemoryManager &mem,
                const OpContext &op_context,
                credb::trusted::ProgramRunner &runner,
                credb::trusted::Ledger &ledger,
                credb::trusted::Peers &peers,
                LockHandle &lock_handle_)
    : cow::Module(mem), m_lock_handle(lock_handle_), m_transaction(IsolationLevel::Serializable, ledger, op_context, &m_lock_handle), m_op_context(op_context), m_runner(runner), m_ledger(ledger), m_peers(peers)
{
}

cow::ValuePtr Transaction::get_member(const std::string &name)
{
    auto &mem = memory_manager();

    if(name == "get_collection")
    {
        return make_value<Function>(mem, [this, &mem](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(args.size() != 1 || args[0]->type() != ValueType::String)
            {
                throw std::runtime_error("Invalid number of arguments");
            }

            auto collection_name = unpack_string(args[0]);
            return make_value<TransactionCollection>(mem, m_op_context, m_transaction, m_ledger, m_lock_handle, collection_name);
        });
    }
    else if(name == "commit_call_on_peer")
    {
       return make_value<Function>(mem, [this, &mem](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(args.size() != 4 || args[0]->type() != ValueType::String ||
               args[1]->type() != ValueType::String || args[2]->type() != ValueType::String ||
               args[3]->type() != ValueType::List)
            {
                throw std::runtime_error("Invalid arguments");
            }

            // call remote program
            auto peer_name = value_cast<StringVal>(args[0])->get();
            auto collection = value_cast<StringVal>(args[1])->get();
            auto full_path = value_cast<StringVal>(args[2])->get();

            auto peer = m_peers.find_by_name(peer_name);

            if(!peer)
            {
                // commit failed...
                auto t = mem.create_tuple();
                t->append(mem.create_boolean(false));
                t->append(mem.create_string("No such peer: " + peer_name));
                return t;
            }

            //TODO add argument
            m_transaction.generate_witness = true;//unpack_bool(args[0]);

            // First validate reads
            if(!m_transaction.phase_one())
            {
                // commit failed...
                auto t = mem.create_tuple();
                t->append(mem.create_boolean(false));
                t->append(mem.create_string("Read are outdated"));

                m_transaction.clear();
                return t;
            }

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

            peer->lock();
            auto op_id = peer->call(collection, m_runner.identifier(), key, path, pargs, true);
            PendingCallResponse pending(op_id, *peer, memory_manager());
            peer->unlock();

            while(!pending.has_message())
            {
                //FIXME thread will suspend here while holding locks
                // This can cause a deadlock if we run out of SGX threads
                m_runner.suspend();

                peer->lock();
                pending.wait(false);
                peer->unlock();
            }

            bool success = pending.success();

            if(success)
            {
                auto val = pending.return_value();

                if(!val || val->type() != ValueType::Bool)
                {
                    success = false;
                }
                else
                {
                    success = cow::unpack_bool(val);
                }
            }
            else if(pending.deadlock_detected())
            {
                log_debug("Possible deadlock detected. backing off...");
                success = false;
            }

            auto t = mem.create_tuple();
            
            if(!success)
            {
                // commit failed...
                t->append(mem.create_boolean(false));
                t->append(mem.create_string("Remote call failed"));
            }
            else
            {

                // Reads are validated and remote call was successful
                // Now we can perform the writes
                
                try {
                    auto witness = m_transaction.phase_two();

                    // build the result structure
                    t->append(mem.create_boolean(true));

                    if(m_transaction.generate_witness)
                    {
                        //FIXME Witness object
                        t->append(mem.create_from_document(witness.digest()));
                    }
                    else
                    {
                        t->append(nullptr);
                    }

                    return t;

                } catch(std::exception &e) {
                    log_error(std::string("Phase two of commit failed unexpectedly: ") + e.what());

                    // commit failed...
                    auto t = mem.create_tuple();
                    t->append(mem.create_boolean(false));
                    t->append(mem.create_string(e.what()));
                    return t;
                }
            }
            
            m_transaction.clear();
            return t;
        });
    }
    else if(name == "commit")
    {
       return make_value<Function>(mem, [this, &mem](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(!(args.empty() || (args.size() == 1 && args[0]->type() == ValueType::Bool)))
            {
                throw std::runtime_error("Invalid arguments!");
            }

            if(args.empty())
            {
                m_transaction.generate_witness = true;
            }
            else
            {
                m_transaction.generate_witness = unpack_bool(args[0]);
            }

            auto t = mem.create_tuple();

            try {
                auto witness = m_transaction.commit();

                // build the result structure
                t->append(mem.create_boolean(true));

                if(m_transaction.generate_witness)
                {
                    //FIXME Witness object
                    t->append(mem.create_from_document(witness.digest()));
                }
                else
                {
                    t->append(nullptr);
                }

            } catch(std::exception &e) {

                // commit failed...
                t->append(mem.create_boolean(false));
                t->append(mem.create_string(e.what()));
            }

            m_transaction.clear();
            return t;
        });
    }
    else
    {
        throw std::runtime_error("No such member: " + name);
    }

}

}
}
}
