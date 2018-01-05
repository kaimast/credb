#include "credb/defines.h"

#include <gtest/gtest.h>

#include "../src/enclave/MultiMap.h"
#include "../src/enclave/StringIndex.h"
#include "../src/enclave/logging.h"
#include "../src/enclave/Index.h"
#include "../src/server/Disk.h"


namespace credb
{
namespace trusted
{

class PrimitivesTest : public testing::Test
{
};

typedef StringIndex string_index_t; // FIXME: the size was 10KB previously

TEST(PrimitivesTest, to_string)
{
    int i = 42;
    auto str = to_string(i);
    EXPECT_EQ(str, "42");
}

TEST(PrimitivesTest, event_id_order)
{
    shard_id_t shard_a = 1;
    shard_id_t shard_b = 2;

    event_index_t index_a = 18;
    event_index_t index_b = 242;

    block_id_t block_a = 54;
    block_id_t block_b = 55;

    event_id_t e_a = {shard_a, block_a, index_a};
    event_id_t e_b = {shard_b, block_a, index_a};
    event_id_t e_c = {shard_a, block_a, index_b};
    event_id_t e_d = {shard_a, block_b, index_a};

    EXPECT_EQ(OrderResult::Unknown, order(e_a, e_b));
    EXPECT_EQ(OrderResult::OlderThan, order(e_a, e_c));
    EXPECT_EQ(OrderResult::NewerThan, order(e_d, e_a));
    EXPECT_EQ(OrderResult::Equal, order(e_b, e_b));
}

#if 0
TEST(PrimitivesTest, string_index)
{
    const std::string str = "foobar_sokewl";
    event_id_t val = {23,4};

    Enclave enclave;
    string_index_t index(enclave, "test_index_2");

    index.insert(str, val);

    event_id_t out;
    bool success = index.get(str, out);

    EXPECT_TRUE(success);
    EXPECT_EQ(out, val);
}

TEST(PrimitivesTest, string_index_update)
{
    const std::string str = "foo";
    event_id_t val1 = {42, 2};
    event_id_t val2 = {45, 3};

    Enclave enclave;
    string_index_t index(enclave, "test_index_3");

    index.insert(str, val1);
    index.insert(str, val2);

    event_id_t out;
    bool success = index.get(str, out);

    EXPECT_TRUE(success);
    EXPECT_EQ(out, val2);
}

/* FIXME need to make this test a friend class
TEST(PrimitivesTest, string_index_serialize_block)
{
    Enclave enclave;
    string_index_t index(enclave, "test_index_42");
    string_index_t::node_t node(index, 1);
    node.children[5] = 42;

    bitstream bstream;
    node.serialize(bstream);

    bstream.move_to(0);

    string_index_t::node_t node2(index, 1);
    node2.load(bstream);

    EXPECT_EQ(memcmp(node.children, node2.children, sizeof(node.children)), 0);
}*/

TEST(PrimitivesTest, string_index_many)
{
    Enclave enclave;
    string_index_t index(enclave, "test_index_4");

    for(uint16_t i = 1; i < 1000; ++i)
    {
        index.insert(to_string(i), {i,i});
    }

    for(uint16_t i = 1; i < 1000; ++i)
    {
        event_id_t id;
        event_id_t expected = {i, i};

        bool res = index.get(to_string(i), id);

        EXPECT_TRUE(res);
        EXPECT_EQ(id, expected);
    }
}

TEST(PrimitivesTest, string_index_iterate)
{
    event_id_t val = {13,37};

    Enclave enclave;
    string_index_t index(enclave, "test_index_1");

    index.insert("foo", val);
    index.insert("baz", val);
    index.insert("cornell", val);

    auto it = index.begin();

    EXPECT_EQ(it.key(), "baz");
    ++it;
    EXPECT_EQ(it.key(), "cornell");
    ++it;
    EXPECT_EQ(it.key(), "foo");
    ++it;
    EXPECT_TRUE(it.at_end());
}

TEST(PrimitivesTest, string_index_iterate_w_prefix)
{
    event_id_t val = {13,37};

    std::string prefix = "superlongprefixhahahah";

    Enclave enclave;
    string_index_t index(enclave, "test_index_1");

    index.insert(prefix+"foo", val);
    index.insert(prefix+"baz", val);
    index.insert(prefix+"cornell", val);

    auto it = index.begin();

    EXPECT_EQ(it.key(), prefix+"baz");
    ++it;
    EXPECT_EQ(it.key(), prefix+"cornell");
    ++it;
    EXPECT_EQ(it.key(), prefix+"foo");
    ++it;
    EXPECT_TRUE(it.at_end());
}

TEST(PrimitivesTest, string_index_very_large_iterate)
{
    event_id_t val = {13,37};

    size_t NUM_ENTRIES = 20000;
    Enclave enclave;
    string_index_t index(enclave, "test_index_1");

    for(uint32_t i = 0; i < NUM_ENTRIES; ++i)
    {
        auto str = credb::random_object_key(20);
        index.insert(str, val);
    }

    size_t count = 0;
    auto it = index.begin();

    while(!it.at_end())
    {
        count += 1;
        ++it;
    }

    EXPECT_EQ(count, NUM_ENTRIES);
}

TEST(PrimitivesTest, string_index_iterate_set_value)
{
    event_id_t val = {13,37};
    event_id_t val2 = {14,47};

    Enclave enclave;
    string_index_t index(enclave, "test_index_1");

    index.insert("foo", val);
    index.insert("baz", val);
    index.insert("cornell", val);

    auto it = index.begin();

    ++it;
    it.set_value(val2);

    it.clear();

    auto it2 = index.begin();

    EXPECT_EQ(it2.key(), "baz");
    EXPECT_EQ(it2.value(), val);
    ++it2;
    EXPECT_EQ(it2.key(), "cornell");
    EXPECT_EQ(it2.value(), val2);
    ++it2;
    ++it2;
    EXPECT_TRUE(it2.at_end());
}

TEST(PrimitivesTest, string_index_staleness_attack_children)
{
    using snapshot_t = std::vector<std::tuple<std::string, std::vector<uint8_t>>>;
    string_index_t index("staleness_attack_string_index");
    event_id_t val1 = {1, 2, 3}, val2 = {4, 5, 6};
    event_id_t out;
    bool ok;

    index.insert("foo", val1);
    snapshot_t snapshot1;
    for (auto &it : g_disk.m_files)
        if (it.first.find(index.name()) == 0)
            snapshot1.emplace_back(it.first, std::vector<uint8_t>(it.second->data, it.second->data + it.second->size));

    index.insert("fer", val2);
    ok = index.get("foo", out);
    EXPECT_TRUE(ok);
    EXPECT_EQ(out, val1);
    ok = index.get("fer", out);
    EXPECT_TRUE(ok);
    EXPECT_EQ(out, val2);

    for (auto &[filename, data] : snapshot1)
    {
        auto &file = *g_disk.m_files[filename];
        delete [] file.data;
        file.size = data.size();
        file.data = new uint8_t[file.size];
        memcpy(file.data, data.data(), file.size);
    }
    index.m_root_node->write_lock();
    while (index.evict_child(index.m_root_node));
    index.m_root_node->write_unlock();

    ASSERT_THROW(index.get("foo", out), StalenessDetectedException);
}

TEST(PrimitivesTest, string_index_staleness_attack_object)
{
    using snapshot_t = std::vector<std::tuple<std::string, std::vector<uint8_t>>>;
    string_index_t index("staleness_attack_object_string_index");
    event_id_t val1 = {1, 2, 3}, val2 = {4, 5, 6}, val3 = {7, 8, 9};
    event_id_t out;
    bool ok;

    index.insert("foo", val1);
    index.insert("fer", val2);
    snapshot_t snapshot1;
    for (auto &it : g_disk.m_files)
        if (it.first.find(index.name()) == 0)
            snapshot1.emplace_back(it.first, std::vector<uint8_t>(it.second->data, it.second->data + it.second->size));

    ok = index.get("foo", out);
    EXPECT_TRUE(ok);
    EXPECT_EQ(out, val1);
    ok = index.get("fer", out);
    EXPECT_TRUE(ok);
    EXPECT_EQ(out, val2);

    index.insert("foo", val3);
    ok = index.get("foo", out);
    EXPECT_TRUE(ok);
    EXPECT_EQ(out, val3);

    for (auto &[filename, data] : snapshot1)
    {
        auto &file = *g_disk.m_files[filename];
        delete [] file.data;
        file.size = data.size();
        file.data = new uint8_t[file.size];
        memcpy(file.data, data.data(), file.size);
    }
    index.m_root_node->write_lock();
    while (index.evict_child(index.m_root_node));
    index.m_root_node->write_unlock();

    ASSERT_THROW(index.get("foo", out), StalenessDetectedException);
}

/*
TEST(PrimitivesTest, map)
{
    const size_t NUM_OBJS = 500;

    std::map<int32_t, int32_t> expected;
    typedef HashMap<100, 100, 1000, int32_t, int32_t> map_t;
    map_t map("testmap");

    srand(4242);

    for(size_t i = 0; i < NUM_OBJS; ++i)
    {
        int32_t k = rand();
        int32_t v = rand();

        map.insert(k,v);
        expected.emplace(k,v);
    }

    EXPECT_EQ(map.size(), NUM_OBJS);

    for(auto e: expected)
    {
        map_t::iterator_t it;
        map.find(it, e.first);

        EXPECT_EQ(it->key, e.first);
        EXPECT_EQ(it->value, e.second);
    }

    uint32_t count = 0;
    map_t::iterator_t it;
    map.begin(it);
    for(; !it.at_end(); ++it)
    {
        auto e = expected.find(it->key);
        EXPECT_EQ(e->first, it->key);
        EXPECT_EQ(e->second, it->value);
        count++;
    }

    EXPECT_EQ(count, map.size());
    EXPECT_EQ(map.size(), expected.size());
}

TEST(PrimitivesTest, multi_map)
{
    MultiMap<100, 100, 1000, std::string, int32_t> map("test_multi_map");

    for(int32_t i = 0; i < 10; ++i)
    {
        map.insert("foo", i);
    }

    std::vector<int32_t> expected = {0,1,2,3,4,5,6,7,8,9};
    EXPECT_EQ(map.find("foo"), expected);
    EXPECT_EQ(map.find("bar"), std::vector<int32_t>{});
}
*/


TEST(PrimitivesTest, hash_index_matches_query)
{
    Enclave enclave;
    HashIndex index(enclave, "index", "prefix", std::vector<std::string>{"key3"});
    json::Document predicate1("{\"key1\": 1, \"key2\": 2, \"key3\": 3}");
    json::Document predicate2("{\"key1\": 1, \"key2\": 2, \"key3\": {\"$in\": [20, 21, 22, 23, 24]}}");
    json::Document predicate3("{\"key1\": 1, \"key2\": 2, \"key3\": {\"$lt\": 25, \"$gte\": 20}}");
    EXPECT_TRUE(index.matches_query("prefix", predicate1));
    EXPECT_TRUE(index.matches_query("prefix", predicate2));
    EXPECT_FALSE(index.matches_query("prefix", predicate3));
    EXPECT_FALSE(index.matches_query("blahblah", predicate1));
    EXPECT_FALSE(index.matches_query("blahblah", predicate2));
    EXPECT_FALSE(index.matches_query("blahblah", predicate3));
}

#endif
}
}
