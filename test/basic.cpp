#include "credb/Client.h"
#include "credb/Collection.h"

#include <assert.h>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <gtest/gtest.h>

class Basic : public testing::Test
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

json::Document value(int32_t i)
{
    return json::Document("{\"id\":" + std::to_string(i) + ",\"msg\":\"you shall not mutate the blockchain\"}");
}

TEST_F(Basic, nop)
{
    const size_t NUM_OBJECTS = 10*1000;

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

    ASSERT_EQ(c->size(), NUM_OBJECTS);
}

TEST_F(Basic, get_is_set_without_key)
{
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
}

TEST_F(Basic, has_object)
{
    auto res1 = c->has_object("foo");
    
    c->put("foo", json::String("bar"));

    auto res2 = c->has_object("foo");

    EXPECT_FALSE(res1);
    EXPECT_TRUE(res2);
}

TEST_F(Basic, get_is_set)
{
    const size_t NUM_OBJECTS = 10*1000;

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

    ASSERT_EQ(c->size(), NUM_OBJECTS);
}

TEST_F(Basic, count_objects)
{
    const size_t NUM_OBJECTS = 1337;

    std::string objs[NUM_OBJECTS];

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        objs[i] = credb::random_object_key(10);
        c->put(objs[i], value(i));
    }

    EXPECT_EQ(c->count(), NUM_OBJECTS);
}

TEST_F(Basic, search_with_predicates)
{
    json::Document doc1("{\"city\":\"ithaca\", \"flag\":3}");
    json::Document doc2("{\"city\":\"NYC\", \"flag\":2}");

    const std::string key1 = "foo"; 
    const std::string key2 = "bar";
    const size_t expected_find_result = 1;

    c->put(key1, doc1);
    c->put(key2, doc2);

    auto res = c->find(json::Document("{\"flag\":2}"), {"city"});

    ASSERT_EQ(res.size(), expected_find_result);
    const auto& [res_key, res_doc] = res[0];
    ASSERT_EQ(res_key, key2);
    ASSERT_EQ(res_doc, json::Document("{\"city\":\"NYC\"}"));
}

TEST_F(Basic, search)
{
    json::Document doc1("{\"city\":\"ithaca\"}");
    json::Document doc2("{\"city\":\"NYC\"}");

    const std::string key1 = "abc";
    const std::string key2 = "edf";
    const size_t expected_num_found = 2;

    c->put(key1, doc1);
    c->put(key2, doc2);

    auto res = c->find();

    ASSERT_EQ(res.size(), expected_num_found);
    const auto& [res_key, res_doc] = res[0];
    ASSERT_EQ(res_key, key1);
    ASSERT_EQ(res_doc, doc1);
}

TEST_F(Basic, cannot_put_invalid_key)
{
    auto res = c->put("&sdf", json::Document("123"));
    ASSERT_FALSE(res);
}

TEST_F(Basic, get_subvalue)
{
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
}

TEST_F(Basic, update_subvalue)
{
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
}

TEST_F(Basic, put_subvalue)
{
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
}

TEST_F(Basic, insert_huge_document)
{
    bitstream bstream;
    json::Writer writer(bstream);

    writer.start_array("");

    for(uint32_t i = 0; i < 10000; ++i)
    {
        writer.write_string("", std::to_string(i));
    }

    writer.end_array();

    std::string key = credb::random_object_key(10);

    json::Document input(bstream.data(), bstream.size(), json::DocumentMode::ReadOnly);

    bool result = static_cast<bool>(c->put(key, input));

    ASSERT_TRUE(result);

    auto output = c->get(key);
    ASSERT_EQ(output, input);
}

TEST_F(Basic, update)
{
    const std::string key = "mykey";
    const size_t expected_num_objects = 1;
    const size_t expected_num_versions = 2;
    const size_t expected_diff_size = 1;

    c->put(key, value(0));
    c->put(key, value(1));

    ASSERT_EQ(c->size(), expected_num_objects);

    auto it = c->get_history(key);

    ASSERT_EQ(it.size(), expected_num_versions);
    ASSERT_EQ(it[0], value(1));
    ASSERT_EQ(it[1], value(0));

    auto diff = c->diff(key, credb::INITIAL_VERSION_NO, credb::INITIAL_VERSION_NO+1);

    ASSERT_EQ(diff.size(), expected_diff_size);
    ASSERT_EQ(diff[0].str(), "{\"type\":\"modified\",\"path\":\"id\",\"new_value\":1}");
}

TEST_F(Basic, trigger)
{
    std::mutex change_mutex;
    std::condition_variable_any change_condition;
    bool change = false;

    std::function<void()> func = [&]()
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
}
