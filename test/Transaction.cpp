#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
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

class TransactionTest : public testing::Test
{
    virtual void TearDown()
    {
        auto conn = create_client("test", "testserver", "localhost");
        auto c = conn->get_collection("default");
        c->clear();
    }
};

TEST_F(TransactionTest, create_witness)
{
    auto conn = create_client("test", "testserver", "localhost");
    const size_t NUM_OBJECTS = 10;

    auto t = conn->init_transaction(IsolationLevel::RepeatableRead);
    auto c = t->get_collection("default");

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        auto str = random_object_key(10);
        c->put(str, value(i));
    }

    auto res = t->commit(true);
    EXPECT_TRUE(res.success);

    EXPECT_TRUE(res.witness.valid(conn->get_server_cert()));
}

TEST_F(TransactionTest, update_value)
{
    auto conn = create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");
    c->put("food", json::String("apple"));

    auto t = conn->init_transaction(IsolationLevel::RepeatableRead);
    auto tc = t->get_collection("default");
    tc->put("food", json::String("orange"));
    auto res = t->commit(true);
    EXPECT_TRUE(res.success);

    t = conn->init_transaction(IsolationLevel::Serializable);
    tc = t->get_collection("default");
    tc->put("food", json::String("orange"));
    res = t->commit(true);
    EXPECT_TRUE(res.success);

    EXPECT_EQ(json::String("orange"), c->get("food"));
}

TEST_F(TransactionTest, get_field)
{
    auto conn = create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");
    auto t = conn->init_transaction(IsolationLevel::Serializable);
    auto tc = t->get_collection("default");

    c->put("foobar", json::Document("{\"x\": 5}"));

    auto read_val = tc->get("foobar.x");

    EXPECT_EQ(json::Integer(5), read_val);

    auto res = t->commit(true);
    EXPECT_TRUE(res.success);
}

TEST_F(TransactionTest, has_object)
{
    auto conn = create_client("test", "testserver", "localhost");
    auto t = conn->init_transaction(IsolationLevel::Serializable);
    auto c = t->get_collection("default");

    if(!c->has_object("foobar"))
    {
        c->put("foobar", json::Integer(42));
    }

    auto res = t->commit(true);
    EXPECT_TRUE(res.success);

    EXPECT_TRUE(conn->get_collection("default")->has_object("foobar"));
}

TEST_F(TransactionTest, create_witness2)
{
    auto conn = create_client("test", "testserver", "localhost");
    const size_t NUM_OBJECTS = 1000;
    const size_t NUM_GETS = 10;

    auto c_ = conn->get_collection("default");

    std::vector<std::string> keys;

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        auto str = random_object_key(10);
        c_->put(str, value(i));
        keys.push_back(str);
    }
    
    auto t = conn->init_transaction(IsolationLevel::RepeatableRead);
    auto c = t->get_collection("default");

    for(size_t i = 0; i < NUM_GETS; ++i)
    {
        auto &key = keys[i];
        auto res = c->get(key);
        EXPECT_EQ(res, value(i));
    }

    auto res = t->commit(true);
    EXPECT_TRUE(res.success);

    EXPECT_TRUE(res.witness.valid(conn->get_server_cert()));
    const std::string armor = res.witness.armor();
    std::istringstream ss(armor);
    Witness witness;
    ss >> witness;

    EXPECT_EQ(armor, witness.armor());
}

TEST_F(TransactionTest, create_witness3)
{
    auto conn = create_client("test", "testserver", "localhost");

    auto t = conn->init_transaction(IsolationLevel::RepeatableRead);
    auto c = t->get_collection("default");

    c->find();

    auto res = t->commit(true);
    EXPECT_TRUE(res.success);

    EXPECT_TRUE(res.witness.valid(conn->get_server_cert()));
}

TEST_F(TransactionTest, create_witness4)
{
    auto conn = create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");
    c->put("food", json::String("hi"));
    
    auto t = conn->init_transaction(IsolationLevel::Serializable);
    auto tc = t->get_collection("default");

    tc->find_one(json::Document(""));
    auto food = tc->get("food");

    auto res = t->commit(true);
    EXPECT_TRUE(res.success);

    EXPECT_TRUE(res.witness.valid(conn->get_server_cert()));
}

TEST_F(TransactionTest, cannot_see_uncommitted_data)
{
    auto conn1 = create_client("test", "testserver", "localhost");
    auto conn2 = create_client("test", "testserver", "localhost");
    auto conn3 = create_client("test", "testserver", "localhost");
    auto c1 = conn1->get_collection("default");
    auto c2 = conn2->get_collection("default");

    for (int i = 0; i < 1000; ++i)
    {
        event_id_t eid;
        const auto k = random_object_key(16);
        const auto v = value(rand());
        auto t1 = conn1->init_transaction(IsolationLevel::RepeatableRead);
        auto t3 = conn3->init_transaction(IsolationLevel::RepeatableRead);
        auto tc1 = t1->get_collection("default");
        auto tc3 = t3->get_collection("default");

        tc1->put(k, v);
        ASSERT_ANY_THROW(c1->get(k));
        ASSERT_ANY_THROW(c2->get(k));
        ASSERT_ANY_THROW(tc3->get(k, eid));

        auto res = t1->commit(false);
        ASSERT_TRUE(res.success);

        ASSERT_EQ(c1->get(k), v);
        ASSERT_EQ(c2->get(k), v);
    }
}

TEST_F(TransactionTest, non_conflicting_concurrent_commit)
{
    auto conn1 = create_client("test", "testserver", "localhost");
    auto conn2 = create_client("test", "testserver", "localhost");

    for (int i = 0; i < 1000; ++i)
    {
        const auto k1 = random_object_key(16);
        const auto k2 = random_object_key(16);
        const auto v1 = value(rand());
        const auto v2 = value(rand());

        auto t1 = conn1->init_transaction(IsolationLevel::RepeatableRead);
        auto t2 = conn2->init_transaction(IsolationLevel::RepeatableRead);

        auto c1 = t1->get_collection("default");
        auto c2 = t1->get_collection("default");

        c1->put(k1, v1);
        c2->put(k2, v2);
        
        auto res1 = t1->commit(false);
        auto res2 = t2->commit(false);
        ASSERT_TRUE(res1.success);
        ASSERT_TRUE(res2.success);
    }
}

TEST_F(TransactionTest, no_outdated_read)
{
    auto conn1 = create_client("test", "testserver", "localhost");
    auto conn2 = create_client("test", "testserver", "localhost");

    for (int i = 0; i < 1000; ++i)
    {
        const auto k = random_object_key(16);
        const auto v1 = value(rand());
        const auto v2 = value(rand());
        const auto v3 = value(rand());

        auto c1 = conn1->get_collection("default");

        event_id_t eid1, eid2, eid3;
        eid1 = c1->put(k, v1);
        ASSERT_NE(eid1, INVALID_EVENT);
        std::this_thread::sleep_for(1ms); // FIXME: downstream may not be up-to-date

        auto t = conn2->init_transaction(IsolationLevel::RepeatableRead);
        auto c2 = t->get_collection("default");
        auto doc = c2->get(k, eid2);
        ASSERT_EQ(doc, v1);
        ASSERT_EQ(eid2, eid1);

        eid3 = c1->put(k, v2);
        ASSERT_NE(eid3, INVALID_EVENT);
        ASSERT_NE(eid3, eid1);

        auto res = t->commit(false);
        ASSERT_FALSE(res.success);
        ASSERT_EQ(res.error, "Key [" + k + "] reads outdated value");
    }
}

inline json::Document balance_doc(const int i)
{
    return json::Document("{\"balance\": " + std::to_string(i) + "}");
}

TEST_F(TransactionTest, atomicity_isolation)
{
    const json::integer_t balance1 = 10000000;
    const json::integer_t balance2 = 20000000;
    const json::integer_t total_balance = balance1 + balance2;
    const std::string name1 = random_object_key(16);
    const std::string name2 = random_object_key(16);

    auto func_transfer = [name1, name2] {
        auto conn = create_client("test", "testserver", "localhost");
        for (int i = 0; i < 100; ++i)
        {
            auto t = conn->init_transaction(IsolationLevel::RepeatableRead);
            auto c = t->get_collection("default");

            auto b1 = json::Document(c->get(name1), "balance").as_integer();
            auto b2 = json::Document(c->get(name2), "balance").as_integer();
            const json::integer_t transfer = rand() % 1000;
            c->put(name1, balance_doc(b1 + transfer));
            c->put(name2, balance_doc(b2 - transfer));
            t->commit(false);
            // allow failure
            std::this_thread::sleep_for(1ms);
        }
    };

    auto conn = create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");

    ASSERT_NE(c->put(name1, balance_doc(balance1)), INVALID_EVENT);
    ASSERT_NE(c->put(name2, balance_doc(balance2)), INVALID_EVENT);

    std::vector<std::thread> threads_transfer;
    for (int i = 0; i < 4; ++i)
    {
        threads_transfer.emplace_back(func_transfer);
    }

    for (auto &t : threads_transfer)
    {
        t.join();
    }

    auto b1 = json::Document(c->get(name1), "balance").as_integer();
    auto b2 = json::Document(c->get(name2), "balance").as_integer();
    const auto total = b1 + b2;
    EXPECT_EQ(total_balance, total);
}

void dirty_read(IsolationLevel isolation)
{
    auto conn = create_client("test", "testserver", "localhost");
    auto t1 = conn->init_transaction(isolation);
    auto t2 = conn->init_transaction(isolation);
    const auto key = random_object_key(16);
    const auto value = ::value(0);

    auto c1 = t1->get_collection("default");
    auto c2 = t2->get_collection("default");

    c1->put(key, value);
    ASSERT_ANY_THROW(c2->get(key));

    auto res = t1->commit(false);
    ASSERT_TRUE(res.success);
    ASSERT_EQ(c2->get(key), value);
}

void non_repeatable_read(IsolationLevel isolation)
{
    auto conn = create_client("test", "testserver", "localhost");
    auto t1 = conn->init_transaction(isolation);
    auto t2 = conn->init_transaction(isolation);
    const auto key = random_object_key(16);
    const auto value = ::value(0);

    auto c0 = conn->get_collection("default");
    auto c1 = t1->get_collection("default");
    auto c2 = t2->get_collection("default");

    ASSERT_NE(c0->put(key, value), INVALID_EVENT);
    ASSERT_EQ(c1->get(key), value);
    c2->put(key, ::value(1));
    auto res2 = t2->commit(false);
    ASSERT_TRUE(res2.success);
    auto res1 = t1->commit(false);
    if (isolation < IsolationLevel::RepeatableRead)
    {
        ASSERT_TRUE(res1.success);
    }
    else
    {
        ASSERT_FALSE(res1.success);
        ASSERT_EQ(res1.error, "Key [" + key + "] reads outdated value");
    }
}

void phantom_read(IsolationLevel isolation)
{
    auto conn = create_client("test", "testserver", "localhost");
    auto t1 = conn->init_transaction(isolation);
    auto t2 = conn->init_transaction(isolation);
    auto t3 = conn->init_transaction(isolation);
    auto t4 = conn->init_transaction(isolation);
    const auto key1 = random_object_key(16);
    const auto key2 = random_object_key(16);
    const auto key3 = random_object_key(16);
    const auto value1 = ::value(1);
    const auto value2 = ::value(2);
    const auto value3 = ::value(3);

    auto c0 = conn->get_collection("default");
    auto c1 = t1->get_collection("default");
    auto c2 = t2->get_collection("default");
    auto c3 = t3->get_collection("default");
    auto c4 = t4->get_collection("default");

    ASSERT_NE(c0->put(key1, value1), INVALID_EVENT);
    ASSERT_NE(c0->put(key2, value2), INVALID_EVENT);

    auto v1 = c1->find();
    ASSERT_EQ(v1.size(), 2);
    std::set<std::string> expect_set1{key1, key2};
    std::set<std::string> result_set1;
    for (const auto& [key, doc] : v1)
    {
        (void)doc;
        result_set1.emplace(key);
    }
    ASSERT_EQ(expect_set1, result_set1);

    c2->put(key3, value3);
    auto res2 = t2->commit(false);
    ASSERT_TRUE(res2.success);

    auto res1 = t1->commit(false);
    if (isolation < IsolationLevel::Serializable)
    {
        ASSERT_TRUE(res1.success);
    }
    else
    {
        ASSERT_FALSE(res1.success);
        ASSERT_EQ(res1.error, "Phantom read: key=" + key3);
    }

    auto v3 = c3->find();
    ASSERT_EQ(v3.size(), 3);
    std::set<std::string> expect_set3{key1, key2, key3};
    std::set<std::string> result_set3;
    for (const auto& [key, doc] : v3)
    {
        (void)doc;
        result_set3.emplace(key);
    }
    ASSERT_EQ(expect_set3, result_set3);

    c4->remove(key1);
    auto res4 = t4->commit(false);
    ASSERT_TRUE(res4.success);

    auto res3 = t3->commit(false);
        
    ASSERT_FALSE(res3.success);
    if (isolation < IsolationLevel::Serializable)
    {
        ASSERT_EQ(res3.error, "Key [" + key1 + "] reads outdated value");
    }
    else
    {
        ASSERT_EQ(res3.error, "Phantom read: too few results");
    }
}

TEST_F(TransactionTest, ReadCommitted_disallow_dirty_read)
{
    dirty_read(IsolationLevel::ReadCommitted);
}

TEST_F(TransactionTest, ReadCommitted_allow_non_repeatable_read)
{
    non_repeatable_read(IsolationLevel::ReadCommitted);
}

TEST_F(TransactionTest, ReadCommitted_allow_phantom_read)
{
    phantom_read(IsolationLevel::ReadCommitted);
}

TEST_F(TransactionTest, RepeatableRead_disallow_dirty_read)
{
    dirty_read(IsolationLevel::RepeatableRead);
}

TEST_F(TransactionTest, RepeatableRead_disallow_non_repeatable_read)
{
    non_repeatable_read(IsolationLevel::RepeatableRead);
}

TEST_F(TransactionTest, RepeatableRead_partially_allow_phantom_read)
{
    phantom_read(IsolationLevel::RepeatableRead);
}

TEST_F(TransactionTest, Serializable_disallow_dirty_read)
{
    dirty_read(IsolationLevel::Serializable);
}

TEST_F(TransactionTest, Serializable_disallow_non_repeatable_read)
{
    non_repeatable_read(IsolationLevel::Serializable);
}

TEST_F(TransactionTest, Serializable_disallow_phantom_read)
{
    phantom_read(IsolationLevel::Serializable);
}
