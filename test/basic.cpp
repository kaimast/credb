#include "credb/Client.h"
#include "credb/Collection.h"

#include <assert.h>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <gtest/gtest.h>

class Basic : testing::Test
{
};

json::Document value(int32_t i)
{
    return json::Document("{\"id\":" + std::to_string(i) + ",\"msg\":\"you shall not mutate the blockchain\"}");
}

TEST(Basic, nop)
{
    auto conn = credb::create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");

    const size_t NUM_OBJECTS = 10*1000;

    size_t start_size = c->size();
    std::string objs[NUM_OBJECTS];

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        objs[i] = credb::random_object_key(10);
        c->put(objs[i], value(i));
    }

    for(size_t i = 0; i < 200000; ++i)
    {
        EXPECT_TRUE(conn->nop(value(i).str()));
    }

    ASSERT_EQ(c->size(), NUM_OBJECTS + start_size);

    c->clear();
}

TEST(Basic, get_is_set_without_key)
{
    auto conn = credb::create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");

    const size_t NUM_OBJECTS = 10*1000;

    size_t start_size = c->size();
    std::string objs[NUM_OBJECTS];

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        auto res = c->put(value(i));
        objs[i] = std::get<0>(res);
    }

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        auto val = c->get(objs[i]);
        ASSERT_EQ(val, value(i));
    }

    ASSERT_EQ(c->size(), NUM_OBJECTS + start_size);

    c->clear();
}

TEST(Basic, get_is_set)
{
    auto conn = credb::create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");

    const size_t NUM_OBJECTS = 10*1000;

    size_t start_size = c->size();
    std::string objs[NUM_OBJECTS];

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        objs[i] = credb::random_object_key(10);
        c->put(objs[i], value(i));
    }

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        auto val = c->get(objs[i]);
        ASSERT_EQ(val, value(i));
    }

    ASSERT_EQ(c->size(), NUM_OBJECTS + start_size);

    c->clear();
}

TEST(Basic, count_objects)
{
    auto conn = credb::create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");

    const size_t NUM_OBJECTS = 1337;

    std::string objs[NUM_OBJECTS];

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        objs[i] = credb::random_object_key(10);
        c->put(objs[i], value(i));
    }

    EXPECT_EQ(c->count(), NUM_OBJECTS);

    c->clear();
}

TEST(Basic, search_with_predicates)
{
    auto conn = credb::create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");

    json::Document doc1("{\"city\":\"ithaca\", \"flag\":3}");
    json::Document doc2("{\"city\":\"NYC\", \"flag\":2}");

    auto key1 = credb::random_object_key(10);
    auto key2 = credb::random_object_key(10);

    c->put(key1, doc1);
    c->put(key2, doc2);

    auto res = c->find(json::Document("{\"flag\":2}"), {"city"});

    ASSERT_EQ(res.size(), 1);
    const auto& [res_key, res_doc] = res[0];
    ASSERT_EQ(res_key, key2);
    ASSERT_EQ(res_doc, json::Document("{\"city\":\"NYC\"}"));

    c->clear();
}

TEST(Basic, search)
{
    auto conn = credb::create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");

    json::Document doc1("{\"city\":\"ithaca\"}");
    json::Document doc2("{\"city\":\"NYC\"}");

    auto key1 = "1_abc";
    auto key2 = "2_edf";

    c->put(key1, doc1);
    c->put(key2, doc2);

    auto res = c->find();

    ASSERT_EQ(res.size(), 2);
    const auto& [res_key, res_doc] = res[0];
    ASSERT_EQ(res_key, key1);
    ASSERT_EQ(res_doc, doc1);

    c->clear();
}

TEST(Basic, cannot_put_invalid_key)
{
    auto conn = credb::create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");

    auto res = c->put("&sdf", json::Document("123"));

    ASSERT_FALSE(res);
}

TEST(Basic, get_subvalue)
{
    auto conn = credb::create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");

    const size_t NUM_OBJECTS = 1000;

    std::string objs[NUM_OBJECTS];

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        objs[i] = credb::random_object_key(10);
        c->put(objs[i], value(i));
    }

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        auto val = c->get(objs[i]+".msg");
        ASSERT_EQ(val, json::Document("\"you shall not mutate the blockchain\""));
    }

    c->clear();
}

TEST(Basic, update_subvalue)
{
    auto conn = credb::create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");

    const size_t NUM_OBJECTS = 1000;

    std::string objs[NUM_OBJECTS];

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        objs[i] = credb::random_object_key(10);
        c->put(objs[i], value(i));
    }

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        bool result = static_cast<bool>(c->put(objs[i] + ".msg", json::Document("\"another message :)\"")));
        ASSERT_TRUE(result);
    }

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        auto val = c->get(objs[i]+".msg");
        ASSERT_EQ(val, json::Document("\"another message :)\""));
    }

    c->clear();
}

TEST(Basic, put_subvalue)
{
    auto conn = credb::create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");

    const size_t NUM_OBJECTS = 1000;

    std::string objs[NUM_OBJECTS];

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        objs[i] = credb::random_object_key(10);
        c->put(objs[i], value(i));
    }

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        bool result = static_cast<bool>(c->put(objs[i] + ".msg2", json::Document("\"another message :)\"")));
        ASSERT_TRUE(result);
    }

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        auto val = c->get(objs[i]+".msg2");
        ASSERT_EQ(val, json::Document("\"another message :)\""));
    }

    c->clear();
}

TEST(Basic, insert_huge_document)
{
    auto conn = credb::create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");

    bitstream bstream;
    json::Writer writer(bstream);

    writer.start_array("");

    for(uint32_t i = 0; i < 10000; ++i) {
        writer.write_string("", std::to_string(i));
    }

    writer.end_array();

    std::string key = credb::random_object_key(10);

    json::Document input(bstream.data(), bstream.size(), json::DocumentMode::ReadOnly);

    bool result = static_cast<bool>(c->put(key, input));

    ASSERT_TRUE(result);

    auto output = c->get(key);
    ASSERT_EQ(output, input);

    c->clear();
}

TEST(Basic, update)
{
    auto conn = credb::create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");

    size_t start_size = c->size();
    const std::string key = credb::random_object_key(10);

    c->put(key, value(0));
    c->put(key, value(1));

    ASSERT_EQ(c->size(), 1 + start_size);

    auto it = c->get_history(key);

    ASSERT_EQ(it.size(), 2);
    ASSERT_EQ(it[0], value(1));
    ASSERT_EQ(it[1], value(0));

    auto diff = c->diff(key, credb::INITIAL_VERSION_NO, credb::INITIAL_VERSION_NO+1);

    ASSERT_EQ(diff.size(), 1);
    ASSERT_EQ(diff[0].str(), "{\"type\":\"modified\",\"path\":\"id\",\"new_value\":1}");

    c->clear();
}

TEST(Basic, trigger)
{
    std::mutex change_mutex;
    std::condition_variable_any change_condition;
    bool change = false;

    auto conn = credb::create_client("test", "testserver", "localhost");
    auto c = conn->get_collection("default");

    std::function<void()> func = [&] () 
    {
        std::lock_guard<std::mutex> lock(change_mutex);
        change = true;
        change_condition.notify_all();
    };
    
    bool success = c->set_trigger(func);
    EXPECT_TRUE(success);

    c->put("foo", json::Integer(2));

    {
        std::unique_lock<std::mutex> lock(change_mutex);
        
        while(!change)
        {
            change_condition.wait(lock);
        }
        
        ASSERT_TRUE(change);
        lock.unlock();
    }

    c->unset_trigger();

    c->clear();
}

