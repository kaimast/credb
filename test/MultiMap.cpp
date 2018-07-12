#include <gtest/gtest.h>

#include "../src/server/Disk.h"
#include "../src/enclave/MultiMap.h"
#include "../src/enclave/BufferManager.h"
#include "../src/enclave/LocalEncryptedIO.h"

#include "credb/defines.h"

using namespace credb;
using namespace credb::trusted;

class MultiMapTest : public testing::Test
{
protected:
    Disk disk;
    LocalEncryptedIO encrypted_io;
    BufferManager *buffer = nullptr;
 
    const size_t buffer_size = 1<<10;
   
    void SetUp() override
    {
        buffer = new BufferManager(&encrypted_io, "test_buffer", buffer_size);
    }

    void TearDown() override
    {
        delete buffer;
    }
};


TEST_F(MultiMapTest, serialize_node)
{
    auto node1 = buffer->new_page<MultiMap::node_t>();
    node1->insert(1, "foo");
    auto no = node1->page_no();
    node1.clear();

    buffer->clear_cache();

    auto node2 = buffer->get_page<MultiMap::node_t>(no);

    std::unordered_set<std::string> out;
    std::unordered_set<std::string> expected = { "foo" };
    
    node2->find_union(1, out);

    EXPECT_EQ(expected, out);
    EXPECT_EQ(node2->size(), static_cast<size_t>(1));
}

TEST_F(MultiMapTest, empty_map)
{
    MultiMap map(*buffer, "foo");
}

TEST_F(MultiMapTest, iterate_one)
{
    const int value = 42;
    const std::string key = "foobar";

    MultiMap map(*buffer, "foo");

    map.insert(value, key);

    EXPECT_EQ(map.size(), size_t(1));

    auto it = map.begin();

    EXPECT_FALSE(it == map.end());
    EXPECT_EQ(value, it.key());
    EXPECT_EQ(key, it.value());

    ++it;
    EXPECT_TRUE(it.at_end());
    EXPECT_TRUE(it == map.end());
}

TEST_F(MultiMapTest, clear)
{
    MultiMap map(*buffer, "foo");
    const int value = 42;
    const std::string key = "foobar";

    map.insert(value, key);
    map.clear();
    map.insert(value, key);

    EXPECT_EQ(map.size(), static_cast<size_t>(1));

    auto it = map.begin();

    EXPECT_FALSE(it == map.end());
    EXPECT_EQ(value, it.key());
    EXPECT_EQ(key, it.value());

    ++it;
    EXPECT_TRUE(it.at_end());
    EXPECT_TRUE(it == map.end());
}

TEST_F(MultiMapTest, iterate_many)
{
    MultiMap map(*buffer, "foo");
    const int value = 42;
    const size_t num_entries = 1000;

    for(size_t i = 0; i < num_entries; ++i)
    {
        map.insert(value, std::to_string(i));
    }

    EXPECT_EQ(map.size(), num_entries);

    auto it = map.begin();

    for(size_t i = 0; i < num_entries; ++i)
    {
        EXPECT_EQ(value, it.key());
        EXPECT_EQ(std::to_string(i), it.value());

        ++it;
    }

    EXPECT_TRUE(it == map.end());
}

TEST_F(MultiMapTest, remove)
{
    MultiMap map(*buffer, "foo");

    map.insert(42, "foobar");
    bool res = map.remove(42, "foobar");

    EXPECT_TRUE(res);
    EXPECT_EQ(map.size(), static_cast<size_t>(0));

    auto it = map.begin();

    EXPECT_TRUE(it == map.end());
    EXPECT_TRUE(it.at_end());
}

TEST_F(MultiMapTest, find_union)
{
    MultiMap map(*buffer, "foo");

    const uint64_t key = rand();
    for(size_t i = 0; i < 10; ++i)
    {
        map.insert(key, std::to_string(i));
    }

    std::unordered_set<std::string> result;

    map.find(key, result, SetOperation::Union);
    auto expected = std::unordered_set<std::string>{"0","1","2","3","4","5","6","7","8","9"};
    EXPECT_EQ(result, expected);
    result.clear();

    map.find(key+1, result, SetOperation::Union);
    EXPECT_EQ(result, std::unordered_set<std::string>{});
    result.clear();
}

TEST_F(MultiMapTest, find_intersect)
{
    MultiMap map(*buffer, "foo");

    const uint64_t key = rand();
    for(size_t i = 0; i < 10; ++i)
    {
        map.insert(key, std::to_string(i));
    }

    std::unordered_set<std::string> result = {"3", "16", "6", "42"};

    map.find(key, result, SetOperation::Intersect);
    auto expected = std::unordered_set<std::string>{"3","6"};
    EXPECT_EQ(result, expected);
    result.clear();

    map.find(key+1, result, SetOperation::Union);
    EXPECT_EQ(result, std::unordered_set<std::string>{});
    result.clear();
}

TEST_F(MultiMapTest, lots_of_data)
{
    MultiMap map(*buffer, "foo");
    const size_t num_entries = 10'000;

    for(size_t i = 0; i < num_entries; ++i)
    {
        map.insert(i, std::to_string(i));
    }

    for(size_t i = 0; i < num_entries; ++i)
    {
        std::unordered_set<std::string> out;
        map.find(i, out, SetOperation::Union);

        std::unordered_set<std::string> expected;
        expected.insert(std::to_string(i));

        ASSERT_EQ(expected, out);
    }
}


TEST_F(MultiMapTest, lots_of_data_random)
{
    MultiMap map(*buffer, "foo");

    std::unordered_map<uint64_t, std::unordered_set<std::string>> data;
    for(size_t i = 0; i < 10000; ++i)
    {
        std::unordered_set<std::string> v;

        int n = rand() % 10;
        uint64_t key = rand();
        for (int j = 0; j < n; ++j)
        {
            std::string value = random_object_key(6);
            v.insert(value);
            map.insert(key, value);
        }

        data.emplace(key, std::move(v));
    }

    for (auto &[key, values] : data)
    {
        std::unordered_set<std::string> out;
        map.find(key, out, SetOperation::Union);
        
        ASSERT_EQ(values, out);
    }
}

TEST_F(MultiMapTest, large_bucket)
{
    MultiMap map(*buffer, "foo");

    std::unordered_map<uint64_t, std::unordered_set<std::string>> data;
    std::unordered_set<std::string> v;
    for(size_t i = 0; i < 10; ++i)
    {
        size_t n = rand() % 10000;
        uint64_t key = rand();
        for(size_t j = 0; j < n; ++j)
        {
            std::string value = random_object_key(6);
            v.insert(value);
            map.insert(key, value);
        }
        data.emplace(key, v);
        v.clear();
    }

    for(auto &[key, values] : data)
    {
        map.find(key, v, SetOperation::Union);
        if (v != values)
        {
            std::cout << "v != values" << std::endl;
            map.find(key, v, SetOperation::Union);
        }
        ASSERT_EQ(values, v);
        v.clear();
    }
}
