#include "NestedTransaction.h"

#include "../logging.h"

namespace credb
{
namespace trusted
{
namespace bindings
{

NestedTransaction::NestedTransaction(cow::MemoryManager &mem, Transaction &parent,
                credb::trusted::Ledger &ledger,
                credb::trusted::Enclave &enclave,
                credb::trusted::ProgramRunner &runner,
                credb::trusted::TransactionPtr transaction,
                credb::trusted::LockHandle &lock_handle)
    : Transaction(mem, ledger, enclave, runner, transaction, lock_handle), m_parent(parent)
{
}

cow::ValuePtr NestedTransaction::get_member(const std::string &name)
{
    using namespace cow;

    auto &mem = memory_manager();

    if(name == "commit")
    {
        return make_value<Function>(mem, [&mem](const std::vector<ValuePtr> &args) -> ValuePtr {
            (void) args;

            auto t = mem.create_tuple();

            t->append(mem.create_boolean(true));
            t->append(nullptr);
 
            return t;
        });
    }
    else
    {
        return m_parent.get_member(name);
    }
}

}
}
}
