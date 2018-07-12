#include <gtest/gtest.h>
#include <iostream>

#include "../src/enclave/LocalEncryptedIO.h"
#include "../src/server/Disk.h"
#include "../src/enclave/HashMap.h"
#include "../src/enclave/BufferManager.h"

using namespace credb;
using namespace credb::trusted;
using string_index_t = credb::trusted::HashMap;

class HashMapTest : public testing::Test
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

TEST_F(HashMapTest, insert_single_and_get)
{
    string_index_t index(*buffer, "test_string_index");

    const std::string str = "foobar_sokewl";
    event_id_t val = {0,23,4};

    index.insert(str, val);

    event_id_t out;
    bool success = index.get(str, out);

    EXPECT_TRUE(success);
    EXPECT_EQ(out, val);
}

TEST_F(HashMapTest, update)
{
    string_index_t index(*buffer, "test_string_index");

    const std::string str = "foo";
    event_id_t val1 = {0, 42, 2};
    event_id_t val2 = {0, 45, 3};

    index.insert(str, val1);
    index.insert(str, val2);

    event_id_t out;
    bool success = index.get(str, out);

    EXPECT_TRUE(success);
    EXPECT_EQ(out, val2);
}

TEST_F(HashMapTest, many)
{
    string_index_t index(*buffer, "test_string_index");

    for(uint16_t i = 1; i < 1000; ++i)
    {
        index.insert(to_string(i), {0, i,i});
    }

    for(uint16_t i = 1; i < 1000; ++i)
    {
        event_id_t id;
        event_id_t expected = {0, i, i};

        bool res = index.get(to_string(i), id);

        EXPECT_TRUE(res);
        EXPECT_EQ(id, expected);
    }
}

TEST_F(HashMapTest, iterate)
{
    string_index_t index(*buffer, "test_string_index");

    event_id_t val = {0, 13,37};

    index.insert("foo", val);
    index.insert("baz", val);
    index.insert("cornell", val);

    auto it = index.begin();

    std::set<std::string> expected = {"foo", "baz", "foobar"};
    std::set<std::string> actual;

    while(!it.at_end())
    {
        actual.insert(it.key());
        ++it;
    }
}

TEST_F(HashMapTest, very_large_iterate)
{
    string_index_t index(*buffer, "test_string_index");

    event_id_t val = {0,13,37};

    size_t NUM_ENTRIES = 20000;

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

TEST_F(HashMapTest, iterate_set_value)
{
    string_index_t index(*buffer, "test_string_index");

    event_id_t val = {0,13,37};
    event_id_t val2 = {0,14,47};

    index.insert("foo", val);

    auto it = index.begin();
    it.set_value(val2);

    it.clear();

    auto it2 = index.begin();

    EXPECT_EQ(it2.key(), "foo");
    EXPECT_EQ(it2.value(), val2);
    ++it2;
    EXPECT_TRUE(it2.at_end());
}

TEST_F(HashMapTest, staleness_attack_object)
{
    using snapshot_t = std::vector<std::tuple<std::string, std::vector<uint8_t>>>;

    string_index_t index(*buffer, "test_string_index");
    event_id_t val1 = {1, 2, 3}, val2 = {4, 5, 6}, val3 = {7, 8, 9};
    event_id_t out;
    bool ok;

    // take a snapshot of outdated data
    index.insert("foo", val1);
    index.insert("fer", val2);
    buffer->flush_all_pages();
    snapshot_t snapshot1;

    for (auto &shard : disk.m_shards)
    {
        for (auto &it : shard.files)
        {
            snapshot1.emplace_back(it.first, std::vector<uint8_t>(it.second->data, it.second->data + it.second->size));
        }
    }

    // make some changes
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

    // unload all pages except for the root node
    buffer->clear_cache();

    // recover from the outdated snapshot
    for (auto &[filename, data] : snapshot1)
    {
        auto &shard = disk.to_shard(filename);
        auto &file  = *shard.files[filename];

        delete [] file.data;
        file.size = data.size();
        file.data = new uint8_t[file.size];
        memcpy(file.data, data.data(), file.size);
    }

    // should detect staleness
    ASSERT_THROW(index.get("foo", out), StalenessDetectedException);
}

TEST_F(HashMapTest, bug)
{
    string_index_t index(*buffer, "test_string_index");

    event_id_t eid;
    ASSERT_NO_THROW(index.insert("key1", eid));
    ASSERT_NO_THROW(index.insert("key1000", eid));
    ASSERT_NO_THROW(index.insert("key1", eid));
    ASSERT_NO_THROW(index.insert("key1000", eid));
}

TEST_F(HashMapTest, serialize_node)
{
    auto node1 = buffer->new_page<HashMap::node_t>();
    node1->increment_version_no();
    node1->increment_version_no();
    node1->increment_version_no();

    auto no = node1->page_no();
    node1.clear();

    buffer->clear_cache();

    auto node2 = buffer->get_page<HashMap::node_t>(no);

    EXPECT_EQ(3, node2->version_no());
}
