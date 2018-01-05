#include <gtest/gtest.h>

#include "credb/Client.h"
#include "credb/Collection.h"

class Workloads : public testing::Test
{
};

TEST(Workloads, random_reads)
{
    constexpr size_t NUM_OBJECTS = 10*1000;
    constexpr size_t NUM_READS = 100;

    auto conn = credb::create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");

    std::vector<std::string> objs;
    objs.resize(NUM_OBJECTS);

    for(uint32_t i = 0; i < NUM_OBJECTS; ++i)
    {
        objs[i] = credb::random_object_key(10);
        c->put(objs[i], json::Document("\"foobar\""));
    }

    for(uint32_t j = 0; j < NUM_READS; ++j)
    {
        auto pos = rand() % NUM_OBJECTS;
        auto res = c->get(objs[pos]);
        EXPECT_EQ(res.as_string(), "foobar");
    }

    c->clear();
}

TEST(Workloads, random_search_wo_index)
{
    constexpr size_t NUM_OBJECTS = 10*1000;
    constexpr size_t NUM_READS = 100;

    auto conn = credb::create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");

    for(uint32_t i = 0; i < NUM_OBJECTS; ++i)
    {
        auto key = credb::random_object_key(10);
        c->put(key, json::Document("{\"value\":" + std::to_string(i) + ", \"msg\":\"go big red\"}"));
    }

    for(uint32_t j = 0; j < NUM_READS; ++j)
    {
        auto pos = rand() % NUM_OBJECTS;
        auto res = c->find_one(json::Document("{\"value\":" + std::to_string(pos) + "}"), {"msg"});
        EXPECT_EQ(std::get<1>(res), json::Document("{\"msg\":\"go big red\"}"));
    }

    c->clear();
}

TEST(Workloads, random_search_with_useless_index)
{
    constexpr size_t NUM_OBJECTS = 10*1000;
    constexpr size_t NUM_READS = 100;

    auto conn = credb::create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");

    for(uint32_t i = 0; i < NUM_OBJECTS; ++i)
    {
        auto key = credb::random_object_key(10);
        c->put(key, json::Document("{\"value\":" + std::to_string(i) + ", \"msg\":\"go big red\"}"));
    }

    c->create_index("useless_index", {"msg"});

    for(uint32_t j = 0; j < NUM_READS; ++j)
    {
        auto pos = rand() % NUM_OBJECTS;
        auto res = c->find_one(json::Document("{\"value\":" + std::to_string(pos) + "}"), {"msg"});
        EXPECT_EQ(std::get<1>(res), json::Document("{\"msg\":\"go big red\"}"));
    }

    EXPECT_TRUE(c->drop_index("useless_index"));
    c->clear();
}

TEST(Workloads, random_search_with_index)
{
    constexpr size_t NUM_OBJECTS = 10*1000;
    constexpr size_t NUM_READS = 100;

    auto conn = credb::create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");

    for(uint32_t i = 0; i < NUM_OBJECTS; ++i)
    {
        auto key = credb::random_object_key(10);
        c->put(key, json::Document("{\"value\":" + std::to_string(i) + ", \"msg\":\"go big red\"}"));
    }

    c->create_index("useful_index", {"value"});

    for(uint32_t j = 0; j < NUM_READS; ++j)
    {
        auto pos = rand() % NUM_OBJECTS;
        auto res = c->find_one(json::Document("{\"value\":" + std::to_string(pos) + "}"), {"msg"});
        EXPECT_EQ(std::get<1>(res), json::Document("{\"msg\":\"go big red\"}"));
    }

    EXPECT_TRUE(c->drop_index("useful_index"));
    c->clear();
}
