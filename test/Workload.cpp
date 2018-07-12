#include <gtest/gtest.h>

#include "credb/Client.h"
#include "credb/Collection.h"

class Workloads : public testing::Test
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

        conn = nullptr;
        c = nullptr;
    }

};

TEST_F(Workloads, random_reads)
{
    constexpr size_t NUM_OBJECTS = 10*1000;
    constexpr size_t NUM_READS = 100;

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
}

TEST_F(Workloads, random_search_wo_index)
{
    constexpr size_t NUM_OBJECTS = 10*1000;
    constexpr size_t NUM_READS = 100;

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
}

TEST_F(Workloads, random_search_with_useless_index)
{
    constexpr size_t NUM_OBJECTS = 10*1000;
    constexpr size_t NUM_READS = 100;

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
}

TEST_F(Workloads, random_search_with_index)
{
    constexpr size_t NUM_OBJECTS = 10*1000;
    constexpr size_t NUM_READS = 100;

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
}
