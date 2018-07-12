#include "../src/enclave/Ledger.h"

#include <gtest/gtest.h>

#include "credb/defines.h"

#include "../src/enclave/TransactionManager.h"
#include "../src/enclave/Enclave.h"
#include "../src/server/Disk.h"

using namespace credb;
using namespace credb::trusted;

const std::string COLLECTION = "test";
const std::string TESTENCLAVE = "test_enclave";

const Identity TEST_IDENTITY = {IdentityType::Client, "test", INVALID_PUBLIC_KEY};
const OpContext TESTSRC = OpContext(TEST_IDENTITY, "");

const Identity OTHER_SERVER = {IdentityType::Server, "other", INVALID_PUBLIC_KEY};

class TransactionManagerTest : public testing::Test
{
protected:
    Disk disk;
    Enclave enclave;
    TransactionManager *tx_mgr = nullptr;

    void SetUp() override
    {
        enclave.init(TESTENCLAVE);
        tx_mgr = &enclave.transaction_manager();
    }
};

TEST_F(TransactionManagerTest, get_tx)
{
    auto tx = tx_mgr->init_local_transaction(IsolationLevel::Serializable);
    auto tx2 = tx_mgr->get(tx->get_root(), tx->identifier());

    EXPECT_EQ(tx_mgr->num_pending_transactions(), size_t(1));
    EXPECT_TRUE(tx2 != nullptr);
}


TEST_F(TransactionManagerTest, abort)
{
    auto tx = tx_mgr->init_local_transaction(IsolationLevel::Serializable);

    tx->abort();
    tx.reset();

    EXPECT_EQ(tx_mgr->num_pending_transactions(), size_t(0));
}

