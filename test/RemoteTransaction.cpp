#include <gtest/gtest.h>
#include "credb/Client.h"
#include "credb/Collection.h"
#include "credb/Transaction.h"
#include "credb/defines.h"
#include <iostream>

#include "../src/enclave/TransactionManager.h"
#include "../src/enclave/Enclave.h"
#include "../src/server/Disk.h"
#include "../src/enclave/op_info.h"

using namespace std::literals;
using namespace credb;
using namespace credb::trusted;

inline json::Document value(int32_t i)
{
    return json::Document("{\"id\":" + std::to_string(i) + ",\"msg\":\"you shall not mutate the blockchain\"}");
}

const std::string COLLECTION = "test";
const std::string TESTENCLAVE = "test_enclave";

const Identity TEST_IDENTITY = {IdentityType::Client, "test", INVALID_PUBLIC_KEY};
const OpContext TESTSRC = OpContext(TEST_IDENTITY, "");

const Identity OTHER_SERVER = {IdentityType::Server, "other", INVALID_PUBLIC_KEY};

class RemoteTransactionTest : public testing::Test
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

TEST_F(RemoteTransactionTest, failed_validation)
{
    auto tx = tx_mgr->init_remote_transaction(enclave.identity().get_unique_id(),1, IsolationLevel::Serializable); 

    auto collection = "collection";
    auto key = "foobar";
    auto res = true;

    auto op = new has_obj_info_t(*tx, collection, key, res, 1);
    tx->register_operation(op);

    auto success = tx->prepare(false);

    EXPECT_FALSE(success);
    EXPECT_EQ(tx->lock_handle().num_locks(), size_t(0));
}

TEST_F(RemoteTransactionTest, abort)
{
    auto tx = tx_mgr->init_remote_transaction(enclave.identity().get_unique_id(),1, IsolationLevel::Serializable); 

    auto collection = "collection";
    auto key = "foobar";
    auto res = false;

    auto op = new has_obj_info_t(*tx, collection, key, res, 1);
    tx->register_operation(op);

    auto success = tx->prepare(false);
    tx->abort();

    EXPECT_TRUE(success);
    EXPECT_EQ(tx->lock_handle().num_locks(), size_t(0));

    tx.reset();

    EXPECT_EQ(tx_mgr->num_pending_transactions(), size_t(0));
}

TEST_F(RemoteTransactionTest, commit)
{
    auto tx = tx_mgr->init_remote_transaction(enclave.identity().get_unique_id(),1, IsolationLevel::Serializable); 

    auto collection = "collection";
    auto key = "foobar";
    auto res = false;

    auto op = new has_obj_info_t(*tx, collection, key, res, 1);
    tx->register_operation(op);

    auto success = tx->prepare(false);
    tx->commit(false);

    EXPECT_TRUE(success);
    EXPECT_EQ(tx->lock_handle().num_locks(), size_t(0));
}
