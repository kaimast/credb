#include <gtest/gtest.h>
#include "credb/Client.h"
#include "credb/Collection.h"
#include "credb/Transaction.h"
#include "credb/defines.h"
#include <iostream>

using namespace std::literals;
using namespace credb;

inline json::Document value(int32_t i)
{
    return json::Document("{\"id\":" + std::to_string(i) + ",\"msg\":\"you shall not mutate the blockchain\"}");
}

class OrderingTest : public testing::Test
{
protected:
    credb::ClientPtr conn = nullptr;
    credb::CollectionPtr c = nullptr;

    void SetUp() override
    {
        conn = credb::create_client("test", "testserver", "localhost");
        c = conn->get_collection("default");
    }

    void TearDown() override
    {
        c->clear();
        conn->close();
    }
};

/**
 * We can order events of the same object locally as it's linearizable
 */ 
TEST_F(OrderingTest, local_order)
{
    auto e1 = c->put("foo", json::String("bar"));
    auto e2 = c->put("foo", json::String("baz"));

    EXPECT_TRUE(e1 < e2);
}

/**
 * Order two versions of independent objects
 * They are part of transactions that are ordered with respect to each other so this must always return the correct result
 */
TEST_F(OrderingTest, tx_order1)
{
    auto t1 = conn->init_transaction();
    auto t2 = conn->init_transaction();

    auto tc1 = t1->get_collection("default");
    auto tc2 = t2->get_collection("default");

    tc1->put("foo1", json::String("bar"));
    tc1->put("common", json::Integer(1));
    t1->commit(false);

    tc2->put("foo2", json::String("baz"));
    tc2->put("common", json::Integer(2));
    t2->commit(false);

    auto [doc1, e1] = c->get_with_eid("foo1");
    auto [doc2, e2] = c->get_with_eid("foo2");

    auto res = conn->order(e1, e2);

    EXPECT_EQ(OrderResult::OlderThan, res);
}

TEST_F(OrderingTest, tx_order2)
{
    auto t1 = conn->init_transaction();
    auto t2 = conn->init_transaction();

    auto tc1 = t1->get_collection("default");
    auto tc2 = t2->get_collection("default");

    tc1->put("foo1", json::String("bar"));
    t1->commit(false);

    tc2->put("foo2", json::String("baz"));
    t2->commit(false);

    auto [doc1, e1] = c->get_with_eid("foo1");
    auto [doc2, e2] = c->get_with_eid("foo2");

    auto res = conn->order(e1, e2);

    // This might break if foo1 and foo2 map to the same shard
    EXPECT_EQ(OrderResult::Unknown, res);
}
