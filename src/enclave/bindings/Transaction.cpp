#include "Transaction.h"

#include "../LockHandle.h"
#include "../Transaction.h"
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
                credb::trusted::Ledger &ledger,
                LockHandle &lock_handle)
    : cow::Module(mem), m_transaction(IsolationLevel::Serializable, ledger, op_context), m_op_context(op_context), m_ledger(ledger), m_lock_handle(lock_handle)
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
    else if(name == "commit")
    {
       return make_value<Function>(mem, [this, &mem](const std::vector<ValuePtr> &args) -> ValuePtr {
            if(args.size() != 1 || args[0]->type() != ValueType::Bool)
            {
                throw std::runtime_error("Invalid arguments!");
            }

            m_transaction.generate_witness = unpack_bool(args[0]);

            try {
                auto witness = m_transaction.commit();

                // build the result structure
                auto t = mem.create_tuple();
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

                // commit failed...
                auto t = mem.create_tuple();
                t->append(mem.create_boolean(false));
                t->append(mem.create_string(e.what()));
                return t;
            }
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
