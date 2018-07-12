#include "../src/enclave/Ledger.h"

#include <gtest/gtest.h>

#include <cowlang/cow.h>
#include <cowlang/unpack.h>

#include "credb/defines.h"

#include "../src/server/Disk.h"
#include "../src/enclave/Enclave.h"

using namespace credb;
using namespace credb::trusted;

const std::string COLLECTION = "test";
const std::string TESTENCLAVE = "test_enclave";

class LockHandleTest: public testing::Test
{
protected:
    Disk disk;
    Enclave enclave;
    Ledger *ledger = nullptr;

    void SetUp() override
    {
        enclave.init(TESTENCLAVE);
        ledger = &enclave.ledger();
    }
};

TEST_F(LockHandleTest, child_destroy)
{
    LockHandle parent(*ledger, nullptr);
    auto child = new LockHandle(*ledger, &parent);

    parent.get_shard(1, LockType::Read);
    child->get_shard(1, LockType::Write);

    EXPECT_EQ(parent.num_locks(), size_t(1));

    delete child;

    EXPECT_EQ(parent.num_locks(), size_t(1));

    parent.release_shard(1, LockType::Read);
    EXPECT_EQ(parent.num_locks(), size_t(0));
}

TEST_F(LockHandleTest, child_clear)
{
    LockHandle parent(*ledger, nullptr);
    LockHandle child(*ledger, &parent);

    parent.get_shard(1, LockType::Read);
    child.get_shard(1, LockType::Write);

    EXPECT_EQ(parent.num_locks(), size_t(1));

    child.clear();

    EXPECT_EQ(parent.num_locks(), size_t(1));

    parent.release_shard(1, LockType::Read);
    EXPECT_EQ(parent.num_locks(), size_t(0));
}

TEST_F(LockHandleTest, multi)
{
    LockHandle hdl(*ledger, nullptr);

    hdl.get_shard(1, LockType::Read);
    hdl.get_shard(11, LockType::Write);

    EXPECT_EQ(hdl.num_locks(), size_t(2));
}

TEST_F(LockHandleTest, upgrade)
{
    LockHandle hdl(*ledger, nullptr);

    hdl.get_shard(1, LockType::Read);
    hdl.get_shard(1, LockType::Write);

    EXPECT_EQ(hdl.num_locks(), size_t(1));

    hdl.release_shard(1, LockType::Write);

    EXPECT_EQ(hdl.num_locks(), size_t(1));

    hdl.release_shard(1, LockType::Read);

    EXPECT_EQ(hdl.num_locks(), size_t(0));
}
 
