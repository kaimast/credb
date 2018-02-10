#include <gtest/gtest.h>
#include "../src/enclave/MultiMap.h"
#include "../src/enclave/BufferManager.h"
#include "credb/defines.h"

using namespace credb;
using namespace credb::trusted;

class MultiMapTest : public testing::Test
{
};

TEST(MultiMapTest, empty_map)
{
    EncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    MultiMap map(buffer, "test_multi_map");
}

TEST(MultiMapTest, iterate_one)
{
    EncryptedIO encrypted_io;
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

TEST(MultiMapTest, iterate_many)
{
    EncryptedIO encrypted_io;
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
    EncryptedIO encrypted_io;
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

TEST(MultiMapTest, find)
{
    EncryptedIO encrypted_io;
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

TEST(MultiMapTest, lots_of_data)
{
    EncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    MultiMap map(buffer, "test_multi_map");

    std::unordered_map<uint64_t, std::unordered_set<std::string>> data;
    std::unordered_set<std::string> v;
    for (int i = 0; i < 10000; ++i)
    {
        int n = rand() % 10;
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

TEST(MultiMapTest, large_bucket)
{
    EncryptedIO encrypted_io;
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
