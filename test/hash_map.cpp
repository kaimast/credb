#include <gtest/gtest.h>
#include "../src/enclave/HashMap.h"

using credb::trusted::HashMap;

typedef HashMap<100*100, 100, 10000, std::string, int64_t> map_t;

class HashMapTest : testing::Test
{
};

TEST(HashMapTest, iterate)
{
    map_t map("testmap_iterate");

    EXPECT_EQ(map.size(), 0);

    map.insert("foo", 42);
    map.insert("bar", 23);

    map_t::iterator_t it;
    map.begin(it);

    EXPECT_EQ(it->value, 42);
    ++it;
    EXPECT_EQ(it->value, 23);
    ++it;

    EXPECT_TRUE(it.at_end());
    EXPECT_EQ(map.size(), 2);
}

TEST(HashMapTest, update)
{
    map_t map("testmap_update");

    map.insert("foo", 42);
    map.insert("foo", 23);

    map_t::iterator_t it;
    map.find(it, "foo");

    EXPECT_EQ(it->value, 23);
    EXPECT_EQ(map.size(), 1);
}

TEST(HashMapTest, get_and_set_many)
{
    const size_t NUM_OBJS = 1;
    map_t map("testmap_get_set_many");

    for(uint32_t i = 0; i < NUM_OBJS; ++i)
        map.insert(std::to_string(i), i);

    for(uint32_t i = 0; i < NUM_OBJS; ++i)
    {
        map_t::iterator_t it;
        map.find(it, std::to_string(i));
        EXPECT_EQ(it->value, i);
        it.clear();
    }
}

TEST(HashMapTest, remove)
{
    map_t map("testmap_remove");

    map.insert("foo", 42);
    map.insert("bar", 23);

    map_t::iterator_t it;
    map.find(it, "bar");
    map.erase(it);

    EXPECT_EQ(map.size(), 1);

    map_t::iterator_t it2;
    map.find(it2, "bar");
    EXPECT_TRUE(it.at_end());
}

TEST(HashMapTest, get_and_set)
{
    map_t map("testmap_get_and_set");

    map.insert("foo", 42);
    map.insert("bar", 23);

    map_t::iterator_t it1, it2;
    map.find(it1, "foo");
    map.find(it2, "bar");

    EXPECT_EQ(it1->value, 42);
    EXPECT_EQ(it2->value, 23);
}
