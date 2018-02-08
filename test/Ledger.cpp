#include "../src/enclave/Ledger.h"

#include <gtest/gtest.h>

#include <cowlang/cow.h>
#include <cowlang/unpack.h>

#include "credb/defines.h"

#include "../src/enclave/Enclave.h"

using namespace credb;
using namespace credb::trusted;

const std::string COLLECTION = "test";
const std::string TESTENCLAVE = "test_enclave";

const Identity TEST_IDENTITY = {IdentityType::Client, "test"};
const OpContext TESTSRC = OpContext(TEST_IDENTITY, "");

class LedgerTest : testing::Test
{
};

TEST(LedgerTest, append_new_array)
{
    json::Document init("{}");
    json::Document add1("1");
    json::Document add2("2");

    const std::string key = "foo";

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    ledger.put(TESTSRC, COLLECTION, key, init);

    ledger.put(TESTSRC, COLLECTION, key, add1, "xyz.+");
    ledger.put(TESTSRC, COLLECTION, key, add2, "xyz.+");

    auto it = ledger.iterate(TESTSRC, COLLECTION, key);
    
    json::Document value("");
    it.next_value(value);

    EXPECT_EQ(value.str(), "{\"xyz\":[1,2]}");
}

TEST(LedgerTest, put_int)
{
    json::Integer intval(1337);
    const std::string key = "foo";

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();
    ledger.put(TESTSRC, COLLECTION, key, intval);

    json::Document expected("1337");

    auto it = ledger.iterate(TESTSRC, COLLECTION, key);

    json::Document value("");
    it.next_value(value);
    EXPECT_EQ(value, expected);
}

TEST(LedgerTest, create_nested_path)
{
    json::Document init("{}");
    json::Document add1("{}");
    json::Document add2("[1,2]");

    const std::string key = "foo";

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    ledger.put(TESTSRC, COLLECTION, key, init);

    ledger.put(TESTSRC, COLLECTION, key, add1, "a.b");
    ledger.put(TESTSRC, COLLECTION, key, add2, "a.b.c");

    auto it = ledger.iterate(TESTSRC, COLLECTION, key);

    json::Document value("");
    it.next_value(value);
    EXPECT_EQ(value.str(), "{\"a\":{\"b\":{\"c\":[1,2]}}}");
}

TEST(LedgerTest, put_path)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    json::Document value1("{\"a\":42}");
    json::Document value2("{\"what\":\"ever\"}");

    const std::string key = "foo";

    ledger.put(TESTSRC, COLLECTION, key, value1);
    ledger.put(TESTSRC, COLLECTION, key, value2, "b");

    auto it = ledger.iterate(TESTSRC, COLLECTION, key);

    json::Document value("");
    it.next_value(value);

    EXPECT_EQ(value.str(), "{\"a\":42,\"b\":{\"what\":\"ever\"}}");
}

/*TEST(LedgerTest, block_size)
{
    Block block(INITIAL_BLOCK);



    auto v = new ObjectVersion("foo", nullptr, 1024, {}, "", INITIAL_VERSION_NO, 0, 0);
    block.insert(v);

    auto v2 = new ObjectVersion("foo2", nullptr, 1024, {}, "", INITIAL_VERSION_NO, 0, 0);
    block.insert(v2);

    EXPECT_EQ(block.byte_size(), v->byte_size() + v2->byte_size());
}*/

TEST(LedgerTest, check)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();
    
    json::Integer val(5);

    ledger.put(TESTSRC, COLLECTION, "foo", val);

    json::Document p1("{ \"$lte\": 5}");

    EXPECT_TRUE(ledger.check(TESTSRC, COLLECTION, "foo", "", p1));
    EXPECT_FALSE(ledger.check(TESTSRC, COLLECTION, "xyz", "", p1));
}

TEST(LedgerTest, check_subvalue)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();
    
    json::Document val("{\"i\": 5}");

    ledger.put(TESTSRC, COLLECTION, "foo", val);

    json::Document p1("{ \"$lte\": 5}");

    EXPECT_TRUE(ledger.check(TESTSRC, COLLECTION, "foo", "i", p1));
    EXPECT_FALSE(ledger.check(TESTSRC, COLLECTION, "xyz", "i", p1));
}

TEST(LedgerTest, remove_and_put)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    const size_t length = 1024;
    uint8_t value[length];

    const std::string key = "foo";

    json::Binary binary(value, length);

    auto dup = binary.duplicate();
    ledger.put(TESTSRC, COLLECTION, key, dup);

    ledger.remove(TESTSRC, COLLECTION, key);
    auto res = ledger.put(TESTSRC, COLLECTION, key, binary);
    EXPECT_TRUE(res);

    auto it = ledger.iterate(TESTSRC, COLLECTION, key);

    ObjectEventHandle _;
    EXPECT_TRUE(it.next(_));
    it.clear();

    EXPECT_EQ(ledger.num_objects(), 1);
}

TEST(LedgerTest, clear_and_put)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    const size_t length = 1024;
    uint8_t value[length];

    const std::string key = "foo";

    json::Binary binary(value, length);

    auto dup = binary.duplicate();
    ledger.put(TESTSRC, COLLECTION, key, dup);

    ledger.clear(TESTSRC, COLLECTION);

    auto res = ledger.put(TESTSRC, COLLECTION, key, binary);
    EXPECT_TRUE(res);

    auto it = ledger.iterate(TESTSRC, COLLECTION, key);

    ObjectEventHandle _;
    EXPECT_TRUE(it.next(_));
    it.clear();

    EXPECT_EQ(ledger.num_objects(), 1);
}

TEST(LedgerTest, remove)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    const size_t length = 1024;
    uint8_t value[length];

    const std::string key = "foo";

    json::Binary binary(value, length);
    ledger.put(TESTSRC, COLLECTION, key, binary);

    EXPECT_EQ(ledger.num_objects(), 1);

    auto res = ledger.remove(TESTSRC, COLLECTION, key);
    EXPECT_TRUE(res);

    auto it = ledger.iterate(TESTSRC, COLLECTION, key);

    ObjectEventHandle _;
    EXPECT_FALSE(it.next(_));
    it.clear();

    EXPECT_EQ(ledger.num_objects(), 0);
}

TEST(LedgerTest, get_is_set)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    const size_t length = 1024;
    uint8_t binary_val[length];

    const std::string key = "foo";

    json::Binary binary(binary_val, length);
    auto doc = binary.duplicate();

    ledger.put(TESTSRC, COLLECTION, key, doc);
    auto it = ledger.iterate(TESTSRC, COLLECTION, key);

    json::Document value("");
    EXPECT_TRUE(it.next_value(value));
    EXPECT_EQ(value, binary);

    EXPECT_EQ(ledger.num_objects(), 1);
}

TEST(LedgerTest, iterate_none)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    const std::string key = "xyz";
    auto it = ledger.iterate(TESTSRC, COLLECTION, key);

    ObjectEventHandle _;
    EXPECT_FALSE(it.next(_));
}

TEST(LedgerTest, iterate_one)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    const size_t length = 1024;
    uint8_t bin_val[length];

    json::Binary binary(bin_val, length);

    const std::string key = "foo";

    auto doc = binary.duplicate();
    ledger.put(TESTSRC, COLLECTION, key, doc);

    auto it = ledger.iterate(TESTSRC, COLLECTION, key);

    json::Document value("");
    EXPECT_TRUE(it.next_value(value));
    EXPECT_EQ(value, binary);

    ObjectEventHandle _;
    EXPECT_FALSE(it.next(_));
}

TEST(LedgerTest, has_object)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    const size_t length = 1024;
    uint8_t value[length];

    json::Binary binary(value, length);

    const std::string key = "foo";

    ledger.put(TESTSRC, COLLECTION, key, binary);

    EXPECT_TRUE(ledger.has_object(COLLECTION, key));
    EXPECT_EQ(ledger.count_objects(TESTSRC, COLLECTION), 1);
}

TEST(LedgerTest, iterate_two)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    const size_t length = 1024;
    uint8_t value[length];

    json::Binary binary(value, length);

    const std::string key = "foo";

    ledger.put(TESTSRC, COLLECTION, key, binary);
    ledger.put(TESTSRC, COLLECTION, key, binary);

    auto it = ledger.iterate(TESTSRC, COLLECTION, key);

    ObjectEventHandle _;
    EXPECT_TRUE(it.next(_));
}

TEST(LedgerTest, get_is_set_few)
{
    const size_t NUM_OBJECTS = 100;

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();
    uint32_t len = 1024;
    uint8_t value[len];

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        std::string key = "foo" + std::to_string(i);

        json::Binary binary(value, len);
        ledger.put(TESTSRC, COLLECTION, key, binary);
    }

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        auto it = ledger.iterate(TESTSRC, COLLECTION, "foo"+std::to_string(i));

        ObjectEventHandle hdl;
        EXPECT_TRUE(it.next(hdl));
    }

    EXPECT_EQ(ledger.num_objects(), NUM_OBJECTS);
}

TEST(LedgerTest, get_is_set_many)
{
    const size_t NUM_OBJECTS = 1000000;

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();
    uint32_t len = 1024;
    uint8_t value[len];

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        std::string key = std::to_string(i);

        json::Binary binary(value, len);
        ledger.put(TESTSRC, COLLECTION, key, binary);
    }

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        auto it = ledger.iterate(TESTSRC, COLLECTION, std::to_string(i));

        ObjectEventHandle hdl;
        EXPECT_TRUE(it.next(hdl));
    }

    EXPECT_EQ(ledger.num_objects(), NUM_OBJECTS);
}

TEST(LedgerTest, can_iterate_many)
{
    const size_t NUM_OBJECTS = 1000;

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();
    std::string objs[NUM_OBJECTS];

    uint32_t len = 1024;
    uint8_t value[len];

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        json::Binary binary(value, len);
        objs[i] = random_object_key(10);
        ledger.put(TESTSRC, COLLECTION, objs[i], binary);
    }

    for(size_t i = 0; i < NUM_OBJECTS; ++i)
    {
        auto it = ledger.iterate(TESTSRC, COLLECTION, objs[i]);

        ObjectEventHandle _;
        EXPECT_TRUE(it.next(_));
    }

    EXPECT_EQ(ledger.num_objects(), NUM_OBJECTS);
}

TEST(LedgerTest, clear)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    json::Document doc("{\"a\":42, \"b\":23}");

    const std::string key = "foo";
    auto dup = doc.duplicate();
    ledger.put(TESTSRC, COLLECTION, key, dup);

    ledger.clear(TESTSRC, COLLECTION);

    json::Document predicates("{\"b\":23}");

    auto it = ledger.find(TESTSRC, COLLECTION, predicates);

    std::string k;
    ObjectEventHandle v;
    EXPECT_FALSE(it.next(k, v));
}

TEST(LedgerTest, check_names)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    uint32_t length = 42;
    uint8_t data[length];

    json::Binary binary(data, length);

    EXPECT_FALSE(ledger.put(TESTSRC, COLLECTION, "", binary));
    EXPECT_FALSE(ledger.put(TESTSRC, COLLECTION,  "&afb", binary));
    EXPECT_FALSE(ledger.put(TESTSRC, COLLECTION,  "not/a/valid/path", binary));
    EXPECT_TRUE(ledger.put(TESTSRC, COLLECTION,  "a_valid_path", binary));
}

TEST(LedgerTest, multi_find)
{
    constexpr uint32_t NUM_PASSES = 1; // FIXME: too slow, reduce from 10 to 1
    constexpr uint32_t NUM_OBJECTS = 10000;

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    for(uint32_t pass = 0; pass < NUM_PASSES; ++pass)
    {
        auto prefix = "f" + credb::random_object_key(16);

        for(uint32_t i = 0; i < NUM_OBJECTS; ++i)
        {
            auto key = prefix + random_object_key(16);

            json::Document to_put("{\"a\":"+std::to_string(i)+"}");
            auto res = ledger.put(TESTSRC, COLLECTION,  key, to_put);

            EXPECT_TRUE(res);
        }

        {
            json::Document predicate("{\"a\":1337}");

            auto it = ledger.find(TESTSRC, COLLECTION, predicate);

            json::Document expected("{\"a\":1337}");

            std::string key;
            ObjectEventHandle hdl;
            it.next(key, hdl);

            auto value = hdl.value();

            EXPECT_EQ(value, expected);
        }

        EXPECT_EQ(ledger.count_objects(TESTSRC, COLLECTION), NUM_OBJECTS);
        ledger.clear(TESTSRC, COLLECTION);
    }

    json::Document doc("{\"a\":42, \"b\":23}");

    ledger.create_index(COLLECTION, "xyz", {"b"});

    const std::string key = "foo";
    auto dup = doc.duplicate();
    ledger.put(TESTSRC, COLLECTION, key, dup);

    json::Document predicates("{\"b\":23}");

    auto it = ledger.find(TESTSRC, COLLECTION, predicates);

    std::string key_;
    ObjectEventHandle hdl;
    it.next(key_, hdl);

    auto value = hdl.value();

    EXPECT_EQ(value, doc);
}

TEST(LedgerTest, find_with_index)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    json::Document doc("{\"a\":42, \"b\":23}");

    ledger.create_index(COLLECTION, "xyz", {"b"});

    const std::string key = "foo";
    auto dup = doc.duplicate();
    ledger.put(TESTSRC, COLLECTION, key, dup);

    json::Document predicates("{\"b\":23}");

    auto it = ledger.find(TESTSRC, COLLECTION, predicates);

    std::string key_;
    ObjectEventHandle hdl;
    it.next(key_, hdl);

    auto value = hdl.value();
    EXPECT_EQ(value, doc);
}

TEST(LedgerTest, find_array)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    json::Document doc("{\"a\":42, \"b\":23}");

    const std::string key = "foo";
    auto dup = doc.duplicate();

    ledger.create_index(COLLECTION, "xyz", {"b"});
    ledger.put(TESTSRC, COLLECTION, key, dup);
    
    json::Document predicates("{\"b\":{\"$in\":[21,22,23]}}");

    auto it = ledger.find(TESTSRC, COLLECTION, predicates);

    std::string key_;
    ObjectEventHandle hdl;
    bool success = static_cast<bool>(it.next(key_, hdl));

    auto value = hdl.value();

    EXPECT_TRUE(success);
    EXPECT_EQ(value, doc);
}

TEST(LedgerTest, find_with_index2)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    json::Document doc("{\"a\":42, \"b\":23}");

    const std::string key = "foo";
    auto dup = doc.duplicate();
    ledger.put(TESTSRC, COLLECTION, key, dup);

    ledger.create_index(COLLECTION, "xyz", {"b"});

    json::Document predicates("{\"b\":23}");

    auto it = ledger.find(TESTSRC, COLLECTION, predicates);

    std::string key_;
    ObjectEventHandle hdl;
    it.next(key_, hdl);

    auto value =  hdl.value();
    EXPECT_EQ(value, doc);
}

TEST(LedgerTest, find_with_index3)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    json::Document doc1("{\"a\":42, \"b\":23}");
    json::Document doc2("{\"a\":43, \"b\":23}");

    const std::string key = "foo";
    auto dup = doc1.duplicate();
    ledger.put(TESTSRC, COLLECTION, key, dup);
    ledger.put(TESTSRC, COLLECTION, key+"2", doc2);

    ledger.create_index(COLLECTION, "xyz", {"b"});

    json::Document predicates("{\"b\":23, \"a\":42}");

    auto it = ledger.find(TESTSRC, COLLECTION, predicates);

    std::string key_;
    ObjectEventHandle hdl;
    it.next(key_, hdl);

    auto value = hdl.value();

    EXPECT_EQ(value, doc1);
    EXPECT_FALSE(it.next(key_, hdl));
}

TEST(LedgerTest, find_with_index_after_remove1)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    json::Document doc1("{\"a\":42, \"b\":23}");
    json::Document doc2("{\"a\":43, \"b\":23}");

    const std::string key1 = "foo1";
    const std::string key2 = "foo2";
    auto dup = doc1.duplicate();

    ledger.put(TESTSRC, COLLECTION, key1, dup);
    ledger.put(TESTSRC, COLLECTION, key2, doc2);

    ledger.create_index(COLLECTION, "xyz", {"b"});

    ledger.remove(TESTSRC, COLLECTION, key2);

    json::Document predicates("{\"b\":23, \"a\":42}");

    auto it = ledger.find(TESTSRC, COLLECTION, predicates);

    std::string key;
    ObjectEventHandle hdl;
    it.next(key, hdl);

    auto value = hdl.value();

    EXPECT_EQ(value, doc1);
    EXPECT_FALSE(it.next(key, hdl));
}

TEST(LedgerTest, add_after_remove)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    json::Document doc1("{\"a\":42, \"b\":21}");

    const std::string key = "foo1";

    ledger.put(TESTSRC, COLLECTION,  key, doc1);

    ledger.remove(TESTSRC, COLLECTION,  key);

    json::Document to_add("2");
    ledger.add(TESTSRC, COLLECTION, key, to_add, "");

    auto it = ledger.iterate(TESTSRC, COLLECTION,  key);

    json::Document value("");
    it.next_value(value);

    json::Document expected("2");
    EXPECT_EQ(value, expected);

    ObjectEventHandle _;
    EXPECT_FALSE(it.next(_));
}

TEST(LedgerTest, find_with_index_after_update)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    json::Document doc1("{\"a\":42, \"b\":21}");
    json::Document doc2("{\"a\":43, \"b\":23}");

    const std::string key1 = "foo1";
    const std::string key2 = "foo2";

    ledger.put(TESTSRC, COLLECTION, key1, doc1);
    ledger.put(TESTSRC, COLLECTION, key2, doc2);

    ledger.create_index(COLLECTION, "xyz", {"a"});

    json::Document to_add("2");
    ledger.add(TESTSRC, COLLECTION, key1, to_add, "a");

    json::Document predicates("{\"a\":44}");

    auto it = ledger.find(TESTSRC, COLLECTION, predicates);

    std::string key;
    ObjectEventHandle hdl;
    it.next(key, hdl);

    json::Document expected("{\"a\":44, \"b\":21}");
    auto actual = hdl.value();

    EXPECT_EQ(actual, expected);
    EXPECT_FALSE(it.next(key, hdl));
}

TEST(LedgerTest, find_gte)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    json::Document doc1("{\"a\":42, \"b\":23}");
    json::Document doc2("{\"a\":43, \"b\":23}");

    const std::string key = "foo";
    auto dup = doc2.duplicate();
    ledger.put(TESTSRC, COLLECTION, key, doc1);
    ledger.put(TESTSRC, COLLECTION, key+"2", dup);

    json::Document predicates("{\"a\": {\"$gte\":43}}");

    auto it = ledger.find(TESTSRC, COLLECTION, predicates);

    std::string key_;
    ObjectEventHandle hdl;
    it.next(key_, hdl);

    auto actual = hdl.value();

    EXPECT_EQ(actual, doc2);
    EXPECT_FALSE(it.next(key_, hdl));
}

TEST(LedgerTest, find_lt_gte)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    json::Document doc1("{\"a\":[{\"b\":23}, {\"b\":54}]}");
    json::Document doc2("{\"a\":[{\"b\":54}, {\"b\":23}]}");

    const std::string key = "foo";
    ledger.put(TESTSRC, COLLECTION, key, doc1);
    ledger.put(TESTSRC, COLLECTION, key+"2", doc2);

    json::Document predicates("{\"a.*.b\": {\"$lt\":23, \"$gte\":43}}");

    auto it = ledger.find(TESTSRC, COLLECTION, predicates);

    std::string k;
    ObjectEventHandle v;
    EXPECT_FALSE(it.next(k,v));
}

TEST(LedgerTest, find_with_index_after_remove2)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    json::Document doc1("{\"a\":42, \"b\":23}");
    json::Document doc2("{\"a\":43, \"b\":23}");

    const std::string key1 = "foo1";
    const std::string key2 = "foo2";
    auto dup = doc1.duplicate();

    ledger.put(TESTSRC, COLLECTION, key1, doc1);
    ledger.put(TESTSRC, COLLECTION, key2, doc2);

    ledger.create_index(COLLECTION, "xyz", {"b"});

    ledger.remove(TESTSRC, COLLECTION, key1);

    json::Document predicates("{\"b\":23, \"a\":42}");

    auto it = ledger.find(TESTSRC, COLLECTION, predicates);

    std::string k;
    ObjectEventHandle v;
    EXPECT_FALSE(it.next(k,v));
}

TEST(LedgerTest, find_with_index4)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    json::Document doc1("{\"a\":42, \"b\":23}");

    const std::string key = "foo";
    auto dup = doc1.duplicate();
    ledger.put(TESTSRC, COLLECTION, key, dup);

    ledger.create_index(COLLECTION, "xyz", {"a"});

    json::Document predicates("{\"b\":23}");

    auto it = ledger.find(TESTSRC, COLLECTION, predicates);

    std::string key_;
    ObjectEventHandle hdl;
    auto res = it.next(key_, hdl);

    EXPECT_TRUE(res);

    auto actual = hdl.value();

    EXPECT_EQ(actual, doc1);
    EXPECT_FALSE(it.next(key_, hdl));
}

TEST(LedgerTest, block_reference_counting)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    json::Document doc("{\"a\":42, \"b\":23}");

    const std::string key = "foo";
    auto dup = doc.duplicate();
    ledger.put(TESTSRC, COLLECTION,  key, dup);

    {
        auto it = ledger.iterate(TESTSRC, COLLECTION,  key);

        json::Document actual("");
        it.next_value(actual);
        EXPECT_EQ(actual, doc);
    }

    // Iterate should be remove its reference automatically...
    auto res = ledger.put(TESTSRC, COLLECTION,  key, doc);
    EXPECT_TRUE(res);
}

TEST(LedgerTest, find)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    json::Document doc("{\"a\":42, \"b\":23}");

    const std::string key = "foo";
    auto dup = doc.duplicate();
    ledger.put(TESTSRC, COLLECTION, key, dup);

    json::Document predicates("{\"b\":23}");

    auto it = ledger.find(TESTSRC, COLLECTION, predicates);

    std::string key_;
    ObjectEventHandle hdl;
    it.next(key_, hdl);

    auto actual = hdl.value();
    EXPECT_EQ(actual, doc);
}


TEST(LedgerTest, create_witness)
{
    Enclave enclave;
    enclave.init(TESTENCLAVE);

    Ledger &ledger = enclave.ledger();
    std::vector<event_id_t> events;

    std::string key = "entry";

    json::Document doc("[1,2,3,4,5]");
    auto eve = ledger.put(TESTSRC, COLLECTION, key, doc);
    events.push_back(eve);

    credb::Witness witness;
    bool res = ledger.create_witness(witness, events);

    auto shard = ledger.get_shard(COLLECTION, key);

    EXPECT_TRUE(res);
    EXPECT_EQ(witness.digest().str(), "[{\"shard\":" + std::to_string(shard) + ",\"block\":13,\"index\":0,\"source\":\"client://test\",\"type\":\"creation\",\"value\":[1,2,3,4,5]}]");
    EXPECT_TRUE(witness.valid(enclave.public_key()));
}

