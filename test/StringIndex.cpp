#include <gtest/gtest.h>
#include <iostream>
#include "../src/server/Disk.h"
#include "../src/enclave/StringIndex.h"
#include "../src/enclave/BufferManager.h"

using namespace credb;
using namespace credb::trusted;
using string_index_t = credb::trusted::StringIndex;

class StringIndexTest : public testing::Test
{
};

TEST(StringIndexTest, insert_single_and_get)
{
    EncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    string_index_t index(buffer, "test_string_index");

    const std::string str = "foobar_sokewl";
    event_id_t val = {0,23,4};

    index.insert(str, val);

    event_id_t out;
    bool success = index.get(str, out);

    EXPECT_TRUE(success);
    EXPECT_EQ(out, val);
}

TEST(StringIndexTest, update)
{
    EncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    string_index_t index(buffer, "test_string_index");

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

TEST(StringIndexTest, many)
{
    EncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    string_index_t index(buffer, "test_string_index");

    for(uint16_t i = 1; i < 1000; ++i)
        index.insert(to_string(i), {0, i,i});

    for(uint16_t i = 1; i < 1000; ++i)
    {
        event_id_t id;
        event_id_t expected = {0, i, i};

        bool res = index.get(to_string(i), id);

        EXPECT_TRUE(res);
        EXPECT_EQ(id, expected);
    }
}

TEST(StringIndexTest, iterate)
{
    EncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    string_index_t index(buffer, "test_string_index");

    event_id_t val = {0, 13,37};

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

TEST(StringIndexTest, iterate_w_prefix)
{
    EncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    string_index_t index(buffer, "test_string_index");

    event_id_t val = {0,13,37};

    std::string prefix = "superlongprefixhahahah";

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

TEST(StringIndexTest, very_large_iterate)
{
    EncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    string_index_t index(buffer, "test_string_index");

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

TEST(StringIndexTest, iterate_set_value)
{
    EncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    string_index_t index(buffer, "test_string_index");

    event_id_t val = {0,13,37};
    event_id_t val2 = {0,14,47};

    index.insert("foo", val);
    index.insert("baz", val);
    index.insert("cornell", val);

    auto it = index.begin();

    ++it;
    it.set_value(val2, nullptr);

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

TEST(StringIndexTest, string_index_staleness_attack_children)
{
    using snapshot_t = std::vector<std::tuple<std::string, std::vector<uint8_t>>>;
    EncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    string_index_t index(buffer, "test_string_index");
    event_id_t val1 = {1, 2, 3}, val2 = {4, 5, 6};
    event_id_t out;
    bool ok;

    // take a snapshot of outdated data
    index.insert("foo", val1);
    buffer.flush_all_pages();
    snapshot_t snapshot1;
    for (auto &it : g_disk.m_files)
        snapshot1.emplace_back(it.first, std::vector<uint8_t>(it.second->data, it.second->data + it.second->size));

    // make some changes
    index.insert("fer", val2);
    ok = index.get("foo", out);
    EXPECT_TRUE(ok);
    EXPECT_EQ(out, val1);
    ok = index.get("fer", out);
    EXPECT_TRUE(ok);
    EXPECT_EQ(out, val2);

    // unload all pages except for the root node
    buffer.clear_cache();

    // recover from the outdated snapshot
    for (auto &[filename, data] : snapshot1)
    {
        auto &file = *g_disk.m_files[filename];
        delete [] file.data;
        file.size = data.size();
        file.data = new uint8_t[file.size];
        memcpy(file.data, data.data(), file.size);
    }

    // should detect staleness
    ASSERT_THROW(index.get("foo", out), StalenessDetectedException);
}

TEST(StringIndexTest, string_index_staleness_attack_object)
{
    using snapshot_t = std::vector<std::tuple<std::string, std::vector<uint8_t>>>;
    EncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    string_index_t index(buffer, "test_string_index");
    event_id_t val1 = {1, 2, 3}, val2 = {4, 5, 6}, val3 = {7, 8, 9};
    event_id_t out;
    bool ok;

    // take a snapshot of outdated data
    index.insert("foo", val1);
    index.insert("fer", val2);
    buffer.flush_all_pages();
    snapshot_t snapshot1;
    for (auto &it : g_disk.m_files)
        snapshot1.emplace_back(it.first, std::vector<uint8_t>(it.second->data, it.second->data + it.second->size));

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
    buffer.clear_cache();

    // recover from the outdated snapshot
    for (auto &[filename, data] : snapshot1)
    {
        auto &file = *g_disk.m_files[filename];
        delete [] file.data;
        file.size = data.size();
        file.data = new uint8_t[file.size];
        memcpy(file.data, data.data(), file.size);
    }

    // should detect staleness
    ASSERT_THROW(index.get("foo", out), StalenessDetectedException);
}

TEST(StringIndexTest, bug)
{
    EncryptedIO encrypted_io;
    BufferManager buffer(&encrypted_io, "test_buffer", 1<<20);
    string_index_t index(buffer, "test_string_index");
    event_id_t eid;
    ASSERT_NO_THROW(index.insert("key1", eid));
    ASSERT_NO_THROW(index.insert("key1000", eid));
    ASSERT_NO_THROW(index.insert("key1", eid));
    ASSERT_NO_THROW(index.insert("key1000", eid));
}
