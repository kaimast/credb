#include "../src/enclave/Ledger.h"

#include <gtest/gtest.h>

#include <cowlang/cow.h>
#include <cowlang/unpack.h>

#include "credb/defines.h"

#include "../src/server/Disk.h"
#include "../src/enclave/Enclave.h"

using namespace credb;
using namespace credb::trusted;

const std::string COLLECTION = "test";
const std::string TESTENCLAVE = "test_enclave";

const Identity TEST_IDENTITY = {IdentityType::Client, "test", INVALID_PUBLIC_KEY};
const OpContext TESTSRC = OpContext(TEST_IDENTITY, "");

class LedgerTest: public testing::Test
{
protected:
    Disk disk;
    Enclave enclave;
    Ledger *ledger = nullptr;

    void SetUp() override
    {
        enclave.init(TESTENCLAVE);
        ledger = &enclave.ledger();
    }
};

TEST_F(LedgerTest, append_new_array)
{
    json::Document init("{}");
    json::Document add1("1");
    json::Document add2("2");

    const std::string key = "foo";

    ledger->put(TESTSRC, COLLECTION, key, init);
    ledger->put(TESTSRC, COLLECTION, key, add1, "xyz.+");
    ledger->put(TESTSRC, COLLECTION, key, add2, "xyz.+");

    auto it = ledger->iterate(TESTSRC, COLLECTION, key);
    auto [eid, value] = it.next();

    EXPECT_TRUE(eid);
    EXPECT_EQ(value.str(), "{\"xyz\":[1,2]}");
}

TEST_F(LedgerTest, put_int)
{
    json::Integer intval(1337);
    const std::string key = "foo";

    ledger->put(TESTSRC, COLLECTION, key, intval);

    json::Document expected("1337");

    auto it = ledger->iterate(TESTSRC, COLLECTION, key);
    auto [eid, value] = it.next();

    EXPECT_TRUE(eid);
    EXPECT_EQ(value, expected);
}

TEST_F(LedgerTest, create_nested_path)
{
    json::Document init("{}");
    json::Document add1("{}");
    json::Document add2("[1,2]");

    const std::string key = "foo";

    ledger->put(TESTSRC, COLLECTION, key, init);

    ledger->put(TESTSRC, COLLECTION, key, add1, "a.b");
    ledger->put(TESTSRC, COLLECTION, key, add2, "a.b.c");

    auto it = ledger->iterate(TESTSRC, COLLECTION, key);
    auto [eid, value] = it.next();

    EXPECT_TRUE(eid);
    EXPECT_EQ(value.str(), "{\"a\":{\"b\":{\"c\":[1,2]}}}");
}

TEST_F(LedgerTest, put_path)
{
    json::Document value1("{\"a\":42}");
    json::Document value2("{\"what\":\"ever\"}");

    const std::string key = "foo";

    ledger->put(TESTSRC, COLLECTION, key, value1);
    ledger->put(TESTSRC, COLLECTION, key, value2, "b");

    auto it = ledger->iterate(TESTSRC, COLLECTION, key);
    auto [eid, value] = it.next();

    EXPECT_TRUE(eid);
    EXPECT_EQ(value.str(), "{\"a\":42,\"b\":{\"what\":\"ever\"}}");
}

TEST_F(LedgerTest, check)
{
    json::Integer val(5);

    ledger->put(TESTSRC, COLLECTION, "foo", val);

    json::Document p1("{ \"$lte\": 5}");

    EXPECT_TRUE(ledger->check(TESTSRC, COLLECTION, "foo", "", p1));
    EXPECT_FALSE(ledger->check(TESTSRC, COLLECTION, "xyz", "", p1));
}

TEST_F(LedgerTest, check_subvalue)
{
    json::Document val("{\"i\": 5}");

    ledger->put(TESTSRC, COLLECTION, "foo", val);

    json::Document p1("{ \"$lte\": 5}");

    EXPECT_TRUE(ledger->check(TESTSRC, COLLECTION, "foo", "i", p1));
    EXPECT_FALSE(ledger->check(TESTSRC, COLLECTION, "xyz", "i", p1));
}

TEST_F(LedgerTest, remove_and_put)
{
    const size_t length = 1024;
    uint8_t data[length];

    const std::string key = "foo";

    json::Binary binary(data, length);

    auto dup = binary.duplicate();
    ledger->put(TESTSRC, COLLECTION, key, dup);

    ledger->remove(TESTSRC, COLLECTION, key);
    auto res = ledger->put(TESTSRC, COLLECTION, key, binary);
    EXPECT_TRUE(res);

    auto it = ledger->iterate(TESTSRC, COLLECTION, key);
    auto [eid, value] = it.next();

    size_t expected_num_objs = 1;

    EXPECT_TRUE(eid);
    EXPECT_EQ(value, binary); 
    EXPECT_EQ(ledger->num_objects(), expected_num_objs);
}

TEST_F(LedgerTest, clear_and_put)
{
    const size_t length = 1024;
    const size_t expected_size = 1;
    uint8_t value[length];
    const std::string key = "foo";

    json::Binary binary(value, length);

    auto dup = binary.duplicate();
    ledger->put(TESTSRC, COLLECTION, key, dup);

    ledger->clear(TESTSRC, COLLECTION);

    auto res = ledger->put(TESTSRC, COLLECTION, key, binary);
    EXPECT_TRUE(res);

    auto it = ledger->iterate(TESTSRC, COLLECTION, key);
    
    ObjectEventHandle _;
    EXPECT_TRUE(it.next_handle(_));
    EXPECT_EQ(ledger->num_objects(), expected_size);
}

TEST_F(LedgerTest, remove)
{
    const size_t length = 1024;
    uint8_t value[length];
    const std::string key = "foo";
    const size_t expected_num_objects1 = 1;
    const size_t expected_num_objects2 = 0;

    json::Binary binary(value, length);
    ledger->put(TESTSRC, COLLECTION, key, binary);

    EXPECT_EQ(ledger->num_objects(), expected_num_objects1);

    auto res = ledger->remove(TESTSRC, COLLECTION, key);
    EXPECT_TRUE(res);

    auto it = ledger->iterate(TESTSRC, COLLECTION, key);

    ObjectEventHandle _;
    EXPECT_FALSE(it.next_handle(_));
    it.clear();

    EXPECT_EQ(ledger->num_objects(), expected_num_objects2);
}

TEST_F(LedgerTest, update_value)
{
    json::String val1("foo");
    json::String val2("bar");

    const std::string key = "testkey";
    const size_t expected_num_objects = 1;

    ledger->put(TESTSRC, COLLECTION, key, val1);
    ledger->put(TESTSRC, COLLECTION, key, val2);

    auto it = ledger->iterate(TESTSRC, COLLECTION, key);
    auto [eid, value] = it.next();
    
    EXPECT_TRUE(eid);
    EXPECT_EQ(value, json::String("bar"));
    EXPECT_EQ(ledger->num_objects(), expected_num_objects);
}

TEST_F(LedgerTest, get_is_set)
{
    const size_t length = 1024;
    uint8_t binary_val[length];
    const size_t expected_num_objects = 1;
    const std::string key = "foo";

    json::Binary binary(binary_val, length);
    auto doc = binary.duplicate();

    ledger->put(TESTSRC, COLLECTION, key, doc);
    auto it = ledger->iterate(TESTSRC, COLLECTION, key);
    auto [eid, value] = it.next();
    
    EXPECT_TRUE(eid);
    EXPECT_EQ(value, binary);
    EXPECT_EQ(ledger->num_objects(), expected_num_objects);
}

TEST_F(LedgerTest, iterate_none)
{
    const std::string key = "xyz";
    auto it = ledger->iterate(TESTSRC, COLLECTION, key);

    ObjectEventHandle _;
    EXPECT_FALSE(it.next_handle(_));
}

TEST_F(LedgerTest, iterate_one)
{
    const size_t length = 1024;
    uint8_t bin_val[length];

    json::Binary binary(bin_val, length);

    const std::string key = "foo";

    auto doc = binary.duplicate();
    ledger->put(TESTSRC, COLLECTION, key, doc);

    auto it = ledger->iterate(TESTSRC, COLLECTION, key);
    auto [eid, value] = it.next();

    EXPECT_TRUE(eid);
    EXPECT_EQ(value, binary);

    ObjectEventHandle _;
    EXPECT_FALSE(it.next_handle(_));
}

TEST_F(LedgerTest, has_object)
{
    const size_t length = 1024;
    const size_t expected = 1;
    uint8_t value[length];

    json::Binary binary(value, length);

    const std::string key = "foo";

    ledger->put(TESTSRC, COLLECTION, key, binary);

    EXPECT_TRUE(ledger->has_object(COLLECTION, key));
    EXPECT_EQ(ledger->count_objects(TESTSRC, COLLECTION), expected);
}

TEST_F(LedgerTest, iterate_two)
{
    const size_t length = 1024;
    uint8_t value[length];

    json::Binary binary(value, length);

    const std::string key = "foo";

    ledger->put(TESTSRC, COLLECTION, key, binary);
    ledger->put(TESTSRC, COLLECTION, key, binary);

    auto it = ledger->iterate(TESTSRC, COLLECTION, key);

    ObjectEventHandle _;
    EXPECT_TRUE(it.next_handle(_));
}

TEST_F(LedgerTest, get_is_set_few)
{
    const size_t NUM_OBJECTS = 100;

    uint32_t len = 1024;
    uint8_t value[len];

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        std::string key = "foo" + std::to_string(i);

        json::Binary binary(value, len);
        ledger->put(TESTSRC, COLLECTION, key, binary);
    }

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        auto it = ledger->iterate(TESTSRC, COLLECTION, "foo"+std::to_string(i));

        ObjectEventHandle hdl;
        EXPECT_TRUE(it.next_handle(hdl));
    }

    EXPECT_EQ(ledger->num_objects(), NUM_OBJECTS);
}

TEST_F(LedgerTest, get_is_set_many)
{
    const size_t NUM_OBJECTS = 1000000;

    uint32_t len = 1024;
    uint8_t value[len];

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        std::string key = std::to_string(i);

        json::Binary binary(value, len);
        ledger->put(TESTSRC, COLLECTION, key, binary);
    }

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        auto it = ledger->iterate(TESTSRC, COLLECTION, std::to_string(i));

        ObjectEventHandle hdl;
        EXPECT_TRUE(it.next_handle(hdl));
    }

    EXPECT_EQ(ledger->num_objects(), NUM_OBJECTS);
}

TEST_F(LedgerTest, can_iterate_many)
{
    const size_t NUM_OBJECTS = 1000;
    std::string objs[NUM_OBJECTS];

    uint32_t len = 1024;
    uint8_t value[len];

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        json::Binary binary(value, len);
        objs[i] = random_object_key(10);
        ledger->put(TESTSRC, COLLECTION, objs[i], binary);
    }

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        auto it = ledger->iterate(TESTSRC, COLLECTION, objs[i]);

        ObjectEventHandle _;
        EXPECT_TRUE(it.next_handle(_));
    }

    EXPECT_EQ(ledger->num_objects(), NUM_OBJECTS);
}

TEST_F(LedgerTest, clear)
{
    json::Document doc("{\"a\":42, \"b\":23}");

    const std::string key = "foo";
    auto dup = doc.duplicate();
    ledger->put(TESTSRC, COLLECTION, key, dup);

    ledger->clear(TESTSRC, COLLECTION);

    json::Document predicates("{\"b\":23}");

    auto it = ledger->find(TESTSRC, COLLECTION, predicates);

    std::string k;
    ObjectEventHandle v;
    EXPECT_FALSE(it.next(k, v));
}

TEST_F(LedgerTest, check_names)
{
    uint32_t length = 42;
    uint8_t data[length];

    json::Binary binary(data, length);

    EXPECT_FALSE(ledger->put(TESTSRC, COLLECTION, "", binary));
    EXPECT_FALSE(ledger->put(TESTSRC, COLLECTION,  "&afb", binary));
    EXPECT_FALSE(ledger->put(TESTSRC, COLLECTION,  "not/a/valid/path", binary));
    EXPECT_TRUE(ledger->put(TESTSRC, COLLECTION,  "a_valid_path", binary));
}

TEST_F(LedgerTest, multi_find)
{
    constexpr uint32_t NUM_PASSES = 5; 
    constexpr uint32_t NUM_OBJECTS = 10000;

    for(uint32_t pass = 0; pass < NUM_PASSES; ++pass)
    {
        const auto prefix = "f" + credb::random_object_key(16);
        const size_t expected_object_count = 0;

        for(uint32_t i = 0; i < NUM_OBJECTS; ++i)
        {
            auto key = prefix + random_object_key(16);

            json::Document to_put("{\"a\":"+std::to_string(i)+"}");
            auto res = ledger->put(TESTSRC, COLLECTION,  key, to_put);

            EXPECT_TRUE(res);
        }

        {
            json::Document predicate("{\"a\":1337}");

            auto it = ledger->find(TESTSRC, COLLECTION, predicate);

            json::Document expected("{\"a\":1337}");

            std::string key;
            ObjectEventHandle hdl;
            it.next(key, hdl);

            auto value = hdl.value();

            EXPECT_EQ(value, expected);
        }

        EXPECT_EQ(ledger->count_objects(TESTSRC, COLLECTION), NUM_OBJECTS);
        ledger->clear(TESTSRC, COLLECTION);
        EXPECT_EQ(ledger->count_objects(TESTSRC, COLLECTION), expected_object_count);
    }

    json::Document doc("{\"a\":42, \"b\":23}");

    ledger->create_index(COLLECTION, "xyz", {"b"});

    const std::string key = "foo";
    auto dup = doc.duplicate();
    ledger->put(TESTSRC, COLLECTION, key, dup);

    json::Document predicates("{\"b\":23}");

    auto it = ledger->find(TESTSRC, COLLECTION, predicates);

    std::string key_;
    ObjectEventHandle hdl;
    it.next(key_, hdl);

    auto value = hdl.value();

    EXPECT_EQ(value, doc);
}

TEST_F(LedgerTest, find_with_index)
{
    json::Document doc("{\"a\":42, \"b\":23}");

    ledger->create_index(COLLECTION, "xyz", {"b"});

    const std::string key = "foo";
    auto dup = doc.duplicate();
    ledger->put(TESTSRC, COLLECTION, key, dup);

    json::Document predicates("{\"b\":23}");

    auto it = ledger->find(TESTSRC, COLLECTION, predicates);

    std::string key_;
    ObjectEventHandle hdl;
    it.next(key_, hdl);

    auto value = hdl.value();
    EXPECT_EQ(value, doc);
}

TEST_F(LedgerTest, find_array)
{
    json::Document doc("{\"a\":42, \"b\":23}");

    const std::string key = "foo";
    auto dup = doc.duplicate();

    ledger->create_index(COLLECTION, "xyz", {"b"});
    ledger->put(TESTSRC, COLLECTION, key, dup);
    
    json::Document predicates("{\"b\":{\"$in\":[21,22,23]}}");

    auto it = ledger->find(TESTSRC, COLLECTION, predicates);

    std::string key_;
    ObjectEventHandle hdl;
    bool success = static_cast<bool>(it.next(key_, hdl));

    auto value = hdl.value();

    EXPECT_TRUE(success);
    EXPECT_EQ(value, doc);
}

TEST_F(LedgerTest, find_with_index2)
{
    json::Document doc("{\"a\":42, \"b\":23}");

    const std::string key = "foo";
    auto dup = doc.duplicate();
    ledger->put(TESTSRC, COLLECTION, key, dup);

    ledger->create_index(COLLECTION, "xyz", {"b"});

    json::Document predicates("{\"b\":23}");

    auto it = ledger->find(TESTSRC, COLLECTION, predicates);

    std::string key_;
    ObjectEventHandle hdl;
    it.next(key_, hdl);

    auto value =  hdl.value();
    EXPECT_EQ(value, doc);
}

TEST_F(LedgerTest, find_with_index3)
{
    json::Document doc1("{\"a\":42, \"b\":23}");
    json::Document doc2("{\"a\":43, \"b\":23}");

    const std::string key = "foo";
    auto dup = doc1.duplicate();
    ledger->put(TESTSRC, COLLECTION, key, dup);
    ledger->put(TESTSRC, COLLECTION, key+"2", doc2);

    ledger->create_index(COLLECTION, "xyz", {"b"});

    json::Document predicates("{\"b\":23, \"a\":42}");

    auto it = ledger->find(TESTSRC, COLLECTION, predicates);

    std::string key_;
    ObjectEventHandle hdl;
    it.next(key_, hdl);

    auto value = hdl.value();

    EXPECT_EQ(value, doc1);
    EXPECT_FALSE(it.next(key_, hdl));
}

TEST_F(LedgerTest, find_with_index_after_remove1)
{
    json::Document doc1("{\"a\":42, \"b\":23}");
    json::Document doc2("{\"a\":43, \"b\":23}");

    const std::string key1 = "foo1";
    const std::string key2 = "foo2";
    auto dup = doc1.duplicate();

    ledger->put(TESTSRC, COLLECTION, key1, dup);
    ledger->put(TESTSRC, COLLECTION, key2, doc2);

    ledger->create_index(COLLECTION, "xyz", {"b"});

    ledger->remove(TESTSRC, COLLECTION, key2);

    json::Document predicates("{\"b\":23, \"a\":42}");

    auto it = ledger->find(TESTSRC, COLLECTION, predicates);

    std::string key;
    ObjectEventHandle hdl;
    it.next(key, hdl);

    auto value = hdl.value();

    EXPECT_EQ(value, doc1);
    EXPECT_FALSE(it.next(key, hdl));
}

TEST_F(LedgerTest, add_after_remove)
{
    json::Document doc1("{\"a\":42, \"b\":21}");

    const std::string key = "foo1";

    ledger->put(TESTSRC, COLLECTION,  key, doc1);

    ledger->remove(TESTSRC, COLLECTION,  key);

    json::Document to_add("2");
    ledger->add(TESTSRC, COLLECTION, key, to_add, "");

    auto it = ledger->iterate(TESTSRC, COLLECTION,  key);
    auto [eid, value] = it.next();
    (void)eid;

    json::Document expected("2");
    EXPECT_EQ(value, expected);

    ObjectEventHandle _;
    EXPECT_FALSE(it.next_handle(_));
}

TEST_F(LedgerTest, find_with_index_after_update)
{
    json::Document doc1("{\"a\":42, \"b\":21}");
    json::Document doc2("{\"a\":43, \"b\":23}");

    const std::string key1 = "foo1";
    const std::string key2 = "foo2";

    ledger->put(TESTSRC, COLLECTION, key1, doc1);
    ledger->put(TESTSRC, COLLECTION, key2, doc2);

    ledger->create_index(COLLECTION, "xyz", {"a"});

    json::Document to_add("2");
    ledger->add(TESTSRC, COLLECTION, key1, to_add, "a");

    json::Document predicates("{\"a\":44}");

    auto it = ledger->find(TESTSRC, COLLECTION, predicates);

    std::string key;
    ObjectEventHandle hdl;
    it.next(key, hdl);

    json::Document expected("{\"a\":44, \"b\":21}");
    auto actual = hdl.value();

    EXPECT_EQ(actual, expected);
    EXPECT_FALSE(it.next(key, hdl));
}

TEST_F(LedgerTest, find_gte)
{
    json::Document doc1("{\"a\":42, \"b\":23}");
    json::Document doc2("{\"a\":43, \"b\":23}");

    const std::string key = "foo";
    auto dup = doc2.duplicate();
    ledger->put(TESTSRC, COLLECTION, key, doc1);
    ledger->put(TESTSRC, COLLECTION, key+"2", dup);

    json::Document predicates("{\"a\": {\"$gte\":43}}");

    auto it = ledger->find(TESTSRC, COLLECTION, predicates);

    std::string key_;
    ObjectEventHandle hdl;
    it.next(key_, hdl);

    auto actual = hdl.value();

    EXPECT_EQ(actual, doc2);
    EXPECT_FALSE(it.next(key_, hdl));
}

TEST_F(LedgerTest, find_lt_gte)
{
    json::Document doc1("{\"a\":[{\"b\":23}, {\"b\":54}]}");
    json::Document doc2("{\"a\":[{\"b\":54}, {\"b\":23}]}");

    const std::string key = "foo";
    ledger->put(TESTSRC, COLLECTION, key, doc1);
    ledger->put(TESTSRC, COLLECTION, key+"2", doc2);

    json::Document predicates("{\"a.*.b\": {\"$lt\":23, \"$gte\":43}}");

    auto it = ledger->find(TESTSRC, COLLECTION, predicates);

    std::string k;
    ObjectEventHandle v;
    EXPECT_FALSE(it.next(k,v));
}

TEST_F(LedgerTest, find_with_index_after_remove2)
{
    json::Document doc1("{\"a\":42, \"b\":23}");
    json::Document doc2("{\"a\":43, \"b\":23}");

    const std::string key1 = "foo1";
    const std::string key2 = "foo2";
    auto dup = doc1.duplicate();

    ledger->put(TESTSRC, COLLECTION, key1, doc1);
    ledger->put(TESTSRC, COLLECTION, key2, doc2);

    ledger->create_index(COLLECTION, "xyz", {"b"});

    ledger->remove(TESTSRC, COLLECTION, key1);

    json::Document predicates("{\"b\":23, \"a\":42}");

    auto it = ledger->find(TESTSRC, COLLECTION, predicates);

    std::string k;
    ObjectEventHandle v;
    EXPECT_FALSE(it.next(k,v));
}

TEST_F(LedgerTest, find_with_index4)
{
    json::Document doc1("{\"a\":42, \"b\":23}");

    const std::string key = "foo";
    auto dup = doc1.duplicate();
    ledger->put(TESTSRC, COLLECTION, key, dup);

    ledger->create_index(COLLECTION, "xyz", {"a"});

    json::Document predicates("{\"b\":23}");

    auto it = ledger->find(TESTSRC, COLLECTION, predicates);

    std::string key_;
    ObjectEventHandle hdl;
    auto res = it.next(key_, hdl);

    EXPECT_TRUE(res);

    auto actual = hdl.value();

    EXPECT_EQ(actual, doc1);
    EXPECT_FALSE(it.next(key_, hdl));
}

TEST_F(LedgerTest, block_reference_counting)
{
    json::Document doc("{\"a\":42, \"b\":23}");

    const std::string key = "foo";
    auto dup = doc.duplicate();
    ledger->put(TESTSRC, COLLECTION,  key, dup);

    {
        auto it = ledger->iterate(TESTSRC, COLLECTION,  key);

        auto [eid, actual] = it.next();
        
        EXPECT_TRUE(eid);
        EXPECT_EQ(doc, actual);
    }

    // Iterate should be remove its reference automatically...
    auto res = ledger->put(TESTSRC, COLLECTION,  key, doc);
    EXPECT_TRUE(res);
}

TEST_F(LedgerTest, find)
{
    json::Document doc("{\"a\":42, \"b\":23}");

    const std::string key = "foo";
    auto dup = doc.duplicate();
    ledger->put(TESTSRC, COLLECTION, key, dup);

    json::Document predicates("{\"b\":23}");

    auto it = ledger->find(TESTSRC, COLLECTION, predicates);

    std::string key_;
    ObjectEventHandle hdl;
    it.next(key_, hdl);

    auto actual = hdl.value();
    EXPECT_EQ(actual, doc);
}


TEST_F(LedgerTest, create_witness)
{
    std::vector<event_id_t> events;

    std::string key = "entry";

    json::Document doc("[1,2,3,4,5]");
    auto eve = ledger->put(TESTSRC, COLLECTION, key, doc);
    events.push_back(eve);

    credb::Witness witness;
    bool res = ledger->create_witness(witness, events);

    auto shard = ledger->get_shard(COLLECTION, key);

    EXPECT_TRUE(res);
    EXPECT_EQ(witness.digest().str(), "[{\"shard\":" + std::to_string(shard) + ",\"block\":53,\"index\":0,\"source\":\"client://test\",\"type\":\"creation\",\"value\":[1,2,3,4,5]}]");
    EXPECT_TRUE(witness.valid(enclave.public_key()));
}

