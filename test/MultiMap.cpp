#include <gtest/gtest.h>
#include "../src/enclave/MultiMap.h"
#include "../src/enclave/BufferManager.h"
#include "../src/enclave/LocalEncryptedIO.h"

#include "credb/defines.h"

using namespace credb;
using namespace credb::trusted;

class MultiMapTest : public testing::Test
{
};

TEST(MultiMapTest, serialize_node)
{
    LocalEncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);

    auto node1 = buffer.new_page<MultiMap::node_t>();
    node1->insert(1, "foo");
    auto no = node1->page_no();
    node1.clear();

    buffer.clear_cache();

    auto node2 = buffer.get_page<MultiMap::node_t>(no);

    std::unordered_set<std::string> out;
    std::unordered_set<std::string> expected = { "foo" };
    
    node2->find_union(1, out);

    EXPECT_EQ(expected, out);
    EXPECT_EQ(1, node2->size());
}

TEST(MultiMapTest, serialize_node_successor)
{
    LocalEncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);

    auto node1 = buffer.new_page<MultiMap::node_t>();
    auto no = node1->page_no();
    auto succ1 = node1->get_successor(LockType::Write, true, buffer);
    succ1->insert(1, "foo");
    succ1->write_unlock();

    node1.clear();
    succ1.clear();

    buffer.clear_cache();

    auto node2 = buffer.get_page<MultiMap::node_t>(no);
    auto succ2 = node2->get_successor(LockType::Write, false, buffer);

    std::unordered_set<std::string> out;
    std::unordered_set<std::string> expected = { "foo" };
    
    succ2->find_union(1, out);
    succ2->write_unlock();

    EXPECT_EQ(expected, out);
    EXPECT_EQ(0, node2->size());
    EXPECT_EQ(1, succ2->size());
}

TEST(MultiMapTest, empty_map)
{
    LocalEncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    MultiMap map(buffer, "test_multi_map");
}

TEST(MultiMapTest, iterate_one)
{
    LocalEncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    MultiMap map(buffer, "test_multi_map");

    map.insert(42, "foobar");

    EXPECT_EQ(1, map.size());

    auto it = map.begin();

    EXPECT_FALSE(it == map.end());
    EXPECT_EQ(42, it.key());
    EXPECT_EQ("foobar", it.value());

    ++it;
    EXPECT_TRUE(it.at_end());
    EXPECT_TRUE(it == map.end());
}

TEST(MultiMapTest, clear)
{
    LocalEncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    MultiMap map(buffer, "test_multi_map");

    map.insert(42, "foobar");
    map.clear();
    map.insert(42, "foobar");

    EXPECT_EQ(1, map.size());

    auto it = map.begin();

    EXPECT_FALSE(it == map.end());
    EXPECT_EQ(42, it.key());
    EXPECT_EQ("foobar", it.value());

    ++it;
    EXPECT_TRUE(it.at_end());
    EXPECT_TRUE(it == map.end());
}

TEST(MultiMapTest, iterate_many)
{
    LocalEncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    MultiMap map(buffer, "test_multi_map");

    for(uint32_t i = 0; i < 1000; ++i)
    {
        map.insert(42, std::to_string(i));
    }

    EXPECT_EQ(1000, map.size());

    auto it = map.begin();

    for(uint32_t i = 0; i < 1000; ++i)
    {
        EXPECT_EQ(42, it.key());
        EXPECT_EQ(std::to_string(i), it.value());

        ++it;
    }

    EXPECT_TRUE(it == map.end());
}

TEST(MultiMapTest, remove)
{
    LocalEncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    MultiMap map(buffer, "test_multi_map");

    map.insert(42, "foobar");
    bool res = map.remove(42, "foobar");

    EXPECT_TRUE(res);
    EXPECT_EQ(0, map.size());

    auto it = map.begin();

    EXPECT_TRUE(it == map.end());
    EXPECT_TRUE(it.at_end());
}

TEST(MultiMapTest, find_union)
{
    LocalEncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    MultiMap map(buffer, "test_multi_map");

    const uint64_t key = rand();
    for(int i = 0; i < 10; ++i)
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

TEST(MultiMapTest, find_intersect)
{
    LocalEncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    MultiMap map(buffer, "test_multi_map");

    const uint64_t key = rand();
    for(int i = 0; i < 10; ++i)
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

TEST(MultiMapTest, lots_of_data)
{
    LocalEncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    MultiMap map(buffer, "test_multi_map");

    for (int i = 0; i < 10000; ++i)
    {
        map.insert(i, std::to_string(i));
    }

    for (int i = 0; i < 10000; ++i)
    {
        std::unordered_set<std::string> out;
        map.find(i, out, SetOperation::Union);

        std::unordered_set<std::string> expected;
        expected.insert(std::to_string(i));

        ASSERT_EQ(expected, out);
    }
}


TEST(MultiMapTest, lots_of_data_random)
{
    LocalEncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    MultiMap map(buffer, "test_multi_map");

    std::unordered_map<uint64_t, std::unordered_set<std::string>> data;
    for (int i = 0; i < 10000; ++i)
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

TEST(MultiMapTest, large_bucket)
{
    LocalEncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 70<<20);
    MultiMap map(buffer, "test_multi_map");

    std::unordered_map<uint64_t, std::unordered_set<std::string>> data;
    std::unordered_set<std::string> v;
    for (int i = 0; i < 10; ++i)
    {
        int n = rand() % 10000;
        uint64_t key = rand();
        for (int j = 0; j < n; ++j)
        {
            std::string value = random_object_key(6);
            v.insert(value);
            map.insert(key, value);
        }
        data.emplace(key, v);
        v.clear();
    }

    for (auto &[key, values] : data)
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
