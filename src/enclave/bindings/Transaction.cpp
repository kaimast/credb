#include "Transaction.h"

#include "../LockHandle.h"
#include "../TransactionExecutor.h"
#include "../RemotePartyRunner.h"
#include "../Enclave.h"
#include "../PendingCallResponse.h"
#include "../PendingBitstreamResponse.h"
#include "../logging.h"

#include "TransactionCollection.h"
#include "NestedTransaction.h"

#include <cowlang/unpack.h>

using namespace cow;

namespace credb
{
namespace trusted
{
namespace bindings
{

Transaction::Transaction(cow::MemoryManager &mem,
                credb::trusted::Ledger &ledger,
                credb::trusted::Enclave &enclave,
                credb::trusted::ProgramRunner &runner,
                LockHandle &lock_handle_)
    : Database(mem, runner.op_context(), ledger, enclave, &runner, lock_handle_)
{
    if(!has_program_runner())
    {
        throw std::runtime_error("Invalid state: transaction is missing program runner");
    }

    auto &tx_mgr = enclave.transaction_manager();
    m_transaction = tx_mgr.init_local_transaction(IsolationLevel::Serializable);

    m_transaction->init_task(runner.identifier(), runner.op_context());
}

Transaction::Transaction(cow::MemoryManager &mem,
                credb::trusted::Ledger &ledger,
                credb::trusted::Enclave &enclave,
                credb::trusted::ProgramRunner &runner,
                credb::trusted::TransactionPtr transaction,
                LockHandle &lock_handle)
    : Database(mem, runner.op_context(), ledger, enclave, &runner, lock_handle), m_transaction(transaction)
{
    if(!has_program_runner())
    {
        throw std::runtime_error("Invalid state: Transaction is missing program runner");
    }

    m_transaction->init_task(runner.identifier(), runner.op_context());
}

Transaction::~Transaction()
{
    abort();
}

void Transaction::abort()
{
    if(!m_transaction->is_done() && !m_transaction->is_remote())
    {
        TransactionExecutor(m_transaction, m_remote_parties, m_runner).abort();
    }
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
            return make_value<TransactionCollection>(mem, *m_runner, m_transaction, m_ledger, m_lock_handle, collection_name);
        });
    }
    else if(name == "abort")
    {
        return make_value<Function>(mem, [this](const std::vector<ValuePtr> &args) -> ValuePtr {
                (void)args;
                abort();
                return nullptr;
        });
    }
    else if(name == "init_transaction")
    {
        auto &tx = *this;

         return make_value<Function>(mem, [this, &mem, &tx](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(!args.empty())
            {
                throw std::runtime_error("Invalid number of arguments");
            }
       
            return make_value<NestedTransaction>(mem, tx, m_ledger, m_enclave,  *m_runner, m_transaction, m_lock_handle);
        });
    }
    else if(name == "call_on_peer")
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
            auto op_id = peer->call(collection, runner().identifier(),
                    key, path, pargs, m_transaction->get_root(), m_transaction->identifier());
            PendingCallResponse pending(op_id, *peer, m_mem);
            m_transaction->add_child(peer->identity().get_unique_id());

            pending.wait(false);
            
            while(!pending.has_message())
            {
                peer->unlock();
                runner().suspend();
                peer->lock();

                pending.wait(false);
            }

            peer->unlock();

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
    else if(name == "commit" && !m_transaction->is_remote())
    {
       return make_value<Function>(mem, [this, &mem](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(!(args.empty() || (args.size() == 1 && args[0]->type() == ValueType::Bool)))
            {
                throw std::runtime_error("Invalid arguments!");
            }

            bool generate_witness = false; //FIXME

            if(!args.empty())
            {
                generate_witness = unpack_bool(args[0]);
            }

            TransactionExecutor exec(m_transaction, m_remote_parties, m_runner);

            auto t = mem.create_tuple();

            if(!exec.phase_one(generate_witness))
            {
                // commit failed...
                t->append(mem.create_boolean(false));
                t->append(mem.create_string(m_transaction->error()));
            }
            else
            {
                auto witness = exec.phase_two(generate_witness);

                // build the result structure
                t->append(mem.create_boolean(true));

                if(generate_witness)
                {
                    //FIXME Witness object
                    t->append(mem.create_from_document(witness.digest()));
                }
                else
                {
                    t->append(nullptr);
                }
            }

            return t;
        });
    }
    else
    {
        return Database::get_member(name);
    }
}

}
}
}
