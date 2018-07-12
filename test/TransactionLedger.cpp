#include "../src/enclave/Ledger.h"

#include <gtest/gtest.h>

#include "credb/defines.h"

#include "../src/enclave/Enclave.h"
#include "../src/server/Disk.h"

using namespace credb;
using namespace credb::trusted;

const std::string COLLECTION = "test";
const std::string TESTENCLAVE = "test_enclave";

const Identity TEST_IDENTITY = {IdentityType::Client, "test", INVALID_PUBLIC_KEY};
const OpContext TESTSRC = OpContext(TEST_IDENTITY, "");

const Identity OTHER_SERVER = {IdentityType::Server, "other", INVALID_PUBLIC_KEY};

class TransactionLedgerTest : public testing::Test
{
protected:
    Disk disk;
    Enclave enclave;

    const Identity *idty = nullptr;
    TransactionLedger *ledger = nullptr;

    void SetUp() override
    {
        enclave.init(TESTENCLAVE);

        idty = &enclave.identity();
        ledger = &enclave.transaction_ledger();
    }
};

TEST_F(TransactionLedgerTest, set_id)
{
    transaction_id_t tx_id = 1;

    op_set_t local_ops;
    std::unordered_map<identity_uid_t, op_set_t> remote_ops; //empty

    std::unordered_map<taskid_t, OpContext> op_contexts;
    op_contexts.emplace(1, TESTSRC.duplicate());

    auto pos = ledger->insert(op_contexts, idty->get_unique_id(), tx_id, local_ops, remote_ops);

    auto hdl = ledger->get(pos);

    EXPECT_EQ(tx_id, hdl.transaction_id());
}

TEST_F(TransactionLedgerTest, set_local_ops)
{
    transaction_id_t tx_id = 1;

    std::unordered_set<event_id_t> reads = { {1,5,7} };
    std::unordered_set<event_id_t> writes = { {5, 2, 1}, {4, 2, 2}};

    op_set_t local_ops = {reads, writes};
    std::unordered_map<identity_uid_t, op_set_t> remote_ops; //empty

    std::unordered_map<taskid_t, OpContext> op_contexts;
    op_contexts.emplace(1, TESTSRC.duplicate());

    auto pos = ledger->insert(op_contexts, idty->get_unique_id(), tx_id, local_ops, remote_ops);

    auto hdl = ledger->get(pos);

    EXPECT_EQ(local_ops, hdl.local_ops());
}

TEST_F(TransactionLedgerTest, set_remote_ops)
{
    transaction_id_t tx_id = 1;

    std::unordered_map<taskid_t, OpContext> op_contexts;
    op_contexts.emplace(1, TESTSRC.duplicate());

    std::unordered_set<event_id_t> reads = { {1,5,7} };
    std::unordered_set<event_id_t> writes = { {5, 2, 1}, {4, 2, 2}};

    op_set_t local_ops;
    std::unordered_map<identity_uid_t, op_set_t> remote_ops;

    remote_ops[OTHER_SERVER.get_unique_id()] = op_set_t{reads, writes};

    auto pos = ledger->insert(op_contexts, idty->get_unique_id(), tx_id, local_ops, remote_ops);

    auto hdl = ledger->get(pos);

    EXPECT_EQ(remote_ops, hdl.remote_ops());
}
