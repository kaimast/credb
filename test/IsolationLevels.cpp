#include <gtest/gtest.h>

#include "credb/Client.h"
#include "credb/Collection.h"
#include "credb/Transaction.h"
#include "credb/defines.h"

using namespace credb;

class IsolationLevelsTest : public testing::TestWithParam<IsolationLevel>
{
protected:
    void SetUp() override
    {
        isolation = GetParam();
        conn = create_client("test", "testserver", "localhost");
    }

    void TearDown() override
    {
        auto c = conn->get_collection("default");
        c->clear();
        conn->close();
    }

    IsolationLevel isolation;
    std::shared_ptr<credb::Client> conn;
};

inline json::Document value(int32_t i)
{
    return json::Document("{\"id\":" + std::to_string(i) + ",\"msg\":\"you shall not mutate the blockchain\"}");
}

TEST_P(IsolationLevelsTest, dirty_read)
{
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

TEST_P(IsolationLevelsTest, non_repeatable_read)
{
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
        //ASSERT_EQ(res1.error, "Key [" + key + "] reads outdated value");
    }
}


TEST_P(IsolationLevelsTest, phantom_read)
{
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
    ASSERT_EQ(v1.size(), static_cast<size_t>(2));
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
    const size_t expected_num_objs = 3;
    ASSERT_EQ(v3.size(), expected_num_objs);
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
        //ASSERT_EQ(res3.error, "Key [" + key1 + "] reads outdated value");
    }
    else
    {
        ASSERT_EQ(res3.error, "Phantom read: too few results");
    }
}

INSTANTIATE_TEST_CASE_P(IsolationLevelsTest, IsolationLevelsTest,
        testing::Values(IsolationLevel::ReadCommitted, IsolationLevel::RepeatableRead, IsolationLevel::Serializable));
