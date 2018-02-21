#include "../src/enclave/Ledger.h"

#include <gtest/gtest.h>

#include <cowlang/cow.h>
#include <cowlang/unpack.h>

#include "credb/defines.h"

#include "../src/enclave/Enclave.h"
#include "../src/enclave/ProgramRunner.h"


using namespace credb;
using namespace credb::trusted;

const std::string COLLECTION = "test";
const std::string TESTENCLAVE = "test_enclave";

const Identity TEST_IDENTITY = {IdentityType::Client, "test"};
const OpContext TESTSRC = OpContext(TEST_IDENTITY, "");

class ProgramsTest : testing::Test
{
};

TEST(ProgramsTest, call_simple_program)
{
    const std::string program_name = "foo";
    const std::string code = "return True";

    bitstream data = cow::compile_code(code);
    json::Binary binary(data);

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();
    TaskManager &task_manager = enclave.task_manager();

    ledger.put(TESTSRC, COLLECTION, program_name, binary);

    std::vector<std::string> args = {};
 
    auto pdata = ledger.prepare_call(TESTSRC, COLLECTION,  program_name, "");
    auto runner = std::make_shared<ProgramRunner>(enclave, std::move(pdata), COLLECTION, program_name, args);

    task_manager.register_task(runner);
 
    runner->run();
    EXPECT_TRUE(runner->is_done());
    EXPECT_TRUE(cow::unpack_bool(runner->get_result()));
}

TEST(ProgramsTest, read_write_python)
{
    const std::string code = "if 42 == 23:\n"
                             "    return False\n"
                             "else:\n"
                             "    return True";

    bitstream data = cow::compile_code(code);
    json::Binary binary(data);

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();
    TaskManager &task_manager = enclave.task_manager();

    ledger.put(TESTSRC, COLLECTION, "code", binary);

    auto it = ledger.iterate(TESTSRC, COLLECTION,  "code");
    auto [eid, value] = it.next();

    (void)eid;

    std::vector<std::string> args = {};

    auto bs = value.as_bitstream();
    auto runner = std::make_shared<ProgramRunner>(enclave, std::move(bs), COLLECTION, "x", args);

    task_manager.register_task(runner);
    runner->run();

    auto res = runner->get_result();
    EXPECT_TRUE(cow::unpack_bool(res));
}

TEST(ProgramsTest, self_put)
{
    std::string code = "import self\n"
                       "return self.put(\"foo\", \"bar\")";

    bitstream data = cow::compile_code(code);
    json::Binary binary(data);

    json::Writer writer;
    writer.start_map("");
    writer.write_binary("func", data);
    writer.end_map();

    Enclave enclave;
    enclave.init("test");

    Ledger &ledger = enclave.ledger();
    TaskManager &task_manager = enclave.task_manager();

    auto doc = writer.make_document();
    ledger.put(TESTSRC, COLLECTION, "x", doc);

    {
        std::vector<std::string> args = {};

        auto pdata = ledger.prepare_call(TESTSRC, COLLECTION, "x", "func");
        auto runner = std::make_shared<ProgramRunner>(enclave, std::move(pdata), COLLECTION, "x", args);

        task_manager.register_task(runner);
 
        runner->run();
        EXPECT_TRUE(runner->is_done());
        EXPECT_TRUE(cow::unpack_bool(runner->get_result()));
    }

    auto it = ledger.iterate(TESTSRC, COLLECTION, "x", "foo");
    auto [eid, value]= it.next();
    EXPECT_TRUE(eid);

    json::String expected("bar");
    EXPECT_EQ(expected, value);
}

TEST(ProgramsTest, security_policy3)
{
    const Identity _superuser = {IdentityType::Client, "superuser"};
    const Identity _superuser2 = {IdentityType::Client, "superuser2"};

    const OpContext superuser(_superuser);
    const OpContext superuser2(_superuser2);

    std::string code = "import self\n"
                       "from op_context import source_name\n"
                       "from op_info import type\n"
                       "if type == 'put':\n"
                       "    return self.contains('authorized', source_name)\n"
                       "else:\n"
                       "    return True";

    bitstream data = cow::compile_code(code);
    json::Binary binary(data);

    json::Writer writer;
    writer.start_map("");
    writer.write_string("value", "bar");
    writer.write_binary("policy", data);
    writer.start_map("authorized");
    writer.write_integer("superuser", 42);
    writer.end_map();
    writer.end_map();

    json::String val2("xyz");

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    auto doc = writer.make_document();
    ledger.put(TESTSRC, COLLECTION, "foo", doc);

    auto res1 = ledger.put(TESTSRC, COLLECTION,  "foo", val2, "value");
    auto res2 = ledger.put(superuser, COLLECTION,  "foo", val2, "value");

    EXPECT_FALSE(res1);
    EXPECT_TRUE(res2);

    json::Integer val3(0);
    auto auth_update =  ledger.put(superuser, COLLECTION,  "foo", val3, "authorized.superuser2");

    EXPECT_TRUE(auth_update);

    auto res3 = ledger.put(TESTSRC, COLLECTION,  "foo", val2, "value");
    auto res4 = ledger.put(superuser2, COLLECTION,  "foo", val2, "value");

    EXPECT_FALSE(res3);
    EXPECT_TRUE(res4);
}

TEST(ProgramsTest, collection_policy)
{
    const std::string code = "import op_info\n"
                             "return not op_info.is_modification";

    bitstream data = cow::compile_code(code);
    json::Binary binary(data);
    json::String val1("foo");
    json::String val2("bar");

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    auto res1 = ledger.put(TESTSRC, COLLECTION, "key1", val1);
    ledger.put(TESTSRC, COLLECTION, "policy", binary);
    auto res2 = ledger.put(TESTSRC, COLLECTION, "key2", val2);

    EXPECT_TRUE(res1);
    EXPECT_FALSE(res2);
}

TEST(ProgramsTest, get_path_policy)
{
    const std::string code = "from op_info import target\n"
                       "if target == 'foo':\n"
                       "    return True\n"
                       "else:\n"
                       "    return False";

    bitstream data = cow::compile_code(code);
    json::Binary binary(data);

    json::Writer writer;
    writer.start_map("");
    writer.write_string("foo", "bar");
    writer.write_string("bar", "foo");
    writer.write_binary("policy", data);
    writer.end_map();

    json::String val2("xyz");

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    auto doc = writer.make_document();
    ledger.put(TESTSRC, COLLECTION, "foo", doc);

    auto it1 = ledger.iterate(TESTSRC, COLLECTION,  "foo");
    auto [eid1, value1] = it1.next();
    
    (void)value1;
    EXPECT_FALSE(eid1);

    auto it2 = ledger.iterate(TESTSRC, COLLECTION,  "foo", "bar");
    auto [eid2, value2] = it2.next();
 
    (void)value2;
    EXPECT_FALSE(eid2);

    auto it3 = ledger.iterate(TESTSRC, COLLECTION,  "foo", "foo");
    auto [eid3, value3] = it3.next();
 
    EXPECT_TRUE(eid3);

    json::String expected("bar");
    EXPECT_EQ(value3, expected);
}

TEST(ProgramsTest, security_policy)
{
    const std::string code = "from op_info import type\n"
                       "if type == 'put':\n"
                       "    return False\n"
                       "else:\n"
                       "    return True";

    bitstream data = cow::compile_code(code);
    json::Binary binary(data);

    json::Writer writer;
    writer.start_map("");
    writer.write_string("value", "bar");
    writer.write_binary("policy", data);
    writer.end_map();

    json::String val2("xyz");

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    auto doc = writer.make_document();
    ledger.put(TESTSRC, COLLECTION, "foo", doc);

    auto res = ledger.put(TESTSRC, COLLECTION,  "foo", val2, "value");

    EXPECT_FALSE(res);

    auto it = ledger.iterate(TESTSRC, COLLECTION,  "foo", "value");
    auto [eid, value] = it.next();
    
    json::String expected("bar");
    EXPECT_TRUE(eid);
    EXPECT_EQ(value, expected);
}

TEST(ProgramsTest, run_buggy_security_policy)
{
    const Identity _jondoe = {IdentityType::Client, "jondoe"};
    const OpContext jondoe(_jondoe);

    const std::string code = "if invalid_var == 'jondoe':\n"
                       "    return False\n"
                       "else:\n"
                       "    return True";

    bitstream data = cow::compile_code(code);
    json::Binary binary(data);

    json::String val2("xyz");

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    json::Writer writer;
    writer.start_map("");
    writer.write_string("value", "foo");
    writer.write_binary("policy", data);
    writer.end_map();

    auto doc = writer.make_document();
    ledger.put(jondoe, COLLECTION, "foo", doc);

    //Put should fail as security policy crashes
    auto res = ledger.put(jondoe, COLLECTION,  "foo", val2, "value");
    EXPECT_FALSE(res);
}

TEST(ProgramsTest, security_policy2)
{
    Identity _jondoe = {IdentityType::Client, "jondoe"};
    OpContext jondoe(_jondoe);

    const std::string code = "from op_context import source_name\n"
                       "if source_name == 'jondoe':\n"
                       "    return False\n"
                       "else:\n"
                       "    return True";

    bitstream data = cow::compile_code(code);
    json::Binary binary(data);

    json::String val2("xyz");

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    json::Writer writer;
    writer.start_map("");
    writer.write_string("value", "foo");
    writer.write_binary("policy", data);
    writer.end_map();

    auto doc = writer.make_document();
    ledger.put(jondoe, COLLECTION, "foo", doc);

    auto res1 = ledger.put(jondoe, COLLECTION,  "foo", val2, "value");
    EXPECT_FALSE(res1);

    json::String str("xyz");
    auto res2 = ledger.put(TESTSRC, COLLECTION,  "foo", str, "value");
    EXPECT_TRUE(res2);
}

TEST(ProgramsTest, limit_writes_security_policy)
{
    Identity _admin = {IdentityType::Client, "admin"};
    Identity _jane = {IdentityType::Client, "jane"};
    Identity _jon = {IdentityType::Client, "jon"};

    OpContext admin(_admin);
    OpContext jane(_jane);
    OpContext jon(_jon);

    std::string code = "import self\n"
                       "import op_info\n"
                       "from op_context import source_name, source_uri\n"
                       "if not op_info.is_modification:\n"
                       "    return True\n"
                       "if not source_name in ['jane', 'dave']:\n"
                       "    return False #Person not in the user list\n"
                       "# Limit authorized users to one write\n"
                       "num_writes = self.count_writes(source_uri)\n"
                       "if num_writes >= 1:\n"
                       "    return False\n"
                       "else:\n"
                       "    return True";

    auto bin = cow::compile_code(code);

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();

    json::Writer writer;
    writer.start_map("");
    writer.write_integer("value", 42);
    writer.write_binary("policy", bin);
    writer.end_map();

    auto doc = writer.make_document();
    ledger.put(admin, COLLECTION,  "foo", doc);

    for(uint32_t i = 0; i < 5; ++i)
    {
        json::Integer to_add(1);
        ledger.add(jane, COLLECTION, "foo", to_add, "value");
    }

    for(uint32_t i = 0; i < 10; ++i)
    {
        json::Integer to_add(1);
        ledger.add(jon, COLLECTION, "foo", to_add, "value");
    }

    {
        auto it = ledger.iterate(TESTSRC, COLLECTION,  "foo", "value");
        auto [eid, value] = it.next();
    
        EXPECT_TRUE(eid);
        EXPECT_EQ(value, json::Integer(43));
    }

    EXPECT_EQ(ledger.count_writes(TESTSRC, "client://jane", COLLECTION, "foo"), 1);
    EXPECT_EQ(ledger.count_writes(TESTSRC, "client://jon", COLLECTION, "foo"), 0);
}

TEST(ProgramsTest, call_simple_program_nested)
{
    const std::string program_name = "foo";
    const std::string code = "return True";

    bitstream data = cow::compile_code(code);

    json::Writer writer;
    writer.start_map("");
    writer.start_map("a");
    writer.write_binary("b", data);
    writer.end_map();
    writer.end_map();

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();
    TaskManager &task_manager = enclave.task_manager();

    auto doc = writer.make_document();
    ledger.put(TESTSRC, COLLECTION, program_name, doc);

    std::vector<std::string> args = {};
 
    auto pdata = ledger.prepare_call(TESTSRC, COLLECTION, program_name, "a.b");
    auto runner = std::make_shared<ProgramRunner>(enclave, std::move(pdata), COLLECTION, program_name, args);

    task_manager.register_task(runner);
 
    runner->run();
    EXPECT_TRUE(runner->is_done());
    EXPECT_TRUE(cow::unpack_bool(runner->get_result()));
}

TEST(ProgramsTest, run_program_with_get)
{
    const std::string program_name = "foo";
    const std::string code = "import db\n"
                       "c = db.get_collection('test')\n"
                       "sum = 0\n"
                       "for s in ['x', 'y']:\n"
                       "   sum += c.get(s)\n"
                       "return sum > 5";

    bitstream data = cow::compile_code(code);
    json::Binary bin1(data);
    json::Binary bin2(data);

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();
    TaskManager &task_manager = enclave.task_manager();

    json::Integer x(5), y(1);

    ledger.put(TESTSRC, COLLECTION, "x", x);
    ledger.put(TESTSRC, COLLECTION, "y", y);

    std::vector<std::string> args = {};

    auto runner1 = std::make_shared<ProgramRunner>(enclave, bin1.as_bitstream(), COLLECTION, program_name, args);

    task_manager.register_task(runner1);
 
    runner1->run();

    EXPECT_FALSE(runner1->has_errors());
    EXPECT_TRUE(cow::unpack_bool(runner1->get_result()));

    json::Integer x_2(4);
    ledger.put(TESTSRC, COLLECTION, "x", x_2);
    
    auto runner2 = std::make_shared<ProgramRunner>(enclave, bin2.as_bitstream(), COLLECTION, program_name, args);

    task_manager.register_task(runner2);
 
    runner2->run();

    EXPECT_FALSE(cow::unpack_bool(runner2->get_result()));
}

TEST(ProgramsTest, run_buggy_program)
{
    // Make sure the execption isn't propag

    const std::string program_name = "foo";
    const std::string code = "import invalid_module\n"
                       "return sum == 7";

    bitstream data = cow::compile_code(code);
    json::Binary binary(data);

    json::Document i1("{\"val\":3}");
    json::Document i2("{\"val\":4}");

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    TaskManager &task_manager = enclave.task_manager();

    std::vector<std::string> args = {};

    auto runner = std::make_shared<ProgramRunner>(enclave, binary.as_bitstream(), COLLECTION, program_name, args);

    task_manager.register_task(runner);

    runner->run();

    EXPECT_TRUE(runner->has_errors());
}

TEST(ProgramsTest, run_buggy_program2)
{
    // Make sure the execption isn't propag

    const std::string program_name = "foo";
    const std::string code = "import db\n"
                       "unknown_object.call_me()\n"
                       "return sum == 7";

    bitstream data = cow::compile_code(code);
    json::Binary binary(data);

    json::Document i1("{\"val\":3}");
    json::Document i2("{\"val\":4}");

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    TaskManager &task_manager = enclave.task_manager();

    std::vector<std::string> args = {};

    auto runner = std::make_shared<ProgramRunner>(enclave, binary.as_bitstream(), program_name, COLLECTION, args);

    task_manager.register_task(runner);

    runner->run();

    EXPECT_TRUE(runner->has_errors());
}

TEST(ProgramsTest, run_program_with_find)
{
    const std::string program_name = "foo";
    const std::string code = "import db\n"
                       "c = db.get_collection('test')\n"
                       "sum = 0\n"
                       "for k,v in c.find():\n"
                       "    sum += v['val']\n"
                       "return sum == 7";

    bitstream data = cow::compile_code(code);
    json::Binary binary(data);

    json::Document i1("{\"val\":3}");
    json::Document i2("{\"val\":4}");

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();
    TaskManager &task_manager = enclave.task_manager();

    ledger.put(TESTSRC, COLLECTION, "foo1", i1);
    ledger.put(TESTSRC, COLLECTION, "foo2", i2);

    std::vector<std::string> args = {};

    auto runner = std::make_shared<ProgramRunner>(enclave, binary.as_bitstream(), COLLECTION, program_name, args);

    task_manager.register_task(runner);

    runner->run();

    EXPECT_TRUE(runner->get_result());
}

TEST(ProgramsTest, run_program_with_put)
{
    const std::string program_name = "xyzbla";
    const std::string code = "import db\n"
                       "c = db.get_collection('test')\n"
                       "val = 1337\n"
                       "res = c.put('foo', val)\n"
                       "return res";

    bitstream data = cow::compile_code(code);
    json::Binary binary(data);

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();
    TaskManager &task_manager = enclave.task_manager();

    std::vector<std::string> args = {};

    auto runner = std::make_shared<ProgramRunner>(enclave, binary.as_bitstream(), COLLECTION, program_name, args);

    task_manager.register_task(runner);

    runner->run();

    EXPECT_TRUE(runner->get_result());
    
    auto it = ledger.iterate(TESTSRC, COLLECTION, "foo");

    json::Integer expected(1337);

    auto [eid, actual] = it.next();
    EXPECT_TRUE(eid);
    EXPECT_EQ(actual, expected);
}

TEST(ProgramsTest, confidential_chat)
{
    Identity _admin = {IdentityType::Client, "admin"};
    Identity _dave = {IdentityType::Client, "dave"};
    Identity _jane = {IdentityType::Client, "jane"};
    Identity _jon = {IdentityType::Client, "jon"};

    OpContext admin(_admin);
    OpContext dave(_dave);
    OpContext jane(_jane);
    OpContext jon(_jon);


    std::string code = "import self\n"
                       "from op_context import source_name\n"
                       "if not source_name in self.get('participants'):\n"
                       "    return False\n"
                       "else:\n"
                       "    return True";

    auto bin = cow::compile_code(code);
        
    json::Writer writer;
    writer.start_map("");
    writer.start_array("participants");
    writer.write_string("0", "jane");
    writer.write_string("1", "dave");
    writer.end_array();
    writer.start_array("messages");
    writer.end_array();
    writer.write_binary("policy", bin);
    writer.end_map();

    auto doc = writer.make_document();

    Enclave enclave;
    enclave.init(TESTENCLAVE);
    Ledger &ledger = enclave.ledger();
    ledger.put(admin, COLLECTION, "chat", doc);

    for(uint32_t i = 0; i < 5; ++i)
    {
        json::Document to_add(std::to_string(i+1));
        auto res = ledger.put(jane, COLLECTION, "chat", to_add, "messages.+");
        EXPECT_TRUE(res);
    }

    for(uint32_t i = 0; i < 3; ++i)
    {
        json::Document to_add(std::to_string(i+42));
        ledger.put(jon, COLLECTION, "chat", to_add, "messages.+");
    }

    {
        auto it = ledger.iterate(dave, COLLECTION, "chat", "messages");
        auto [eid1, val1] = it.next();
        
        EXPECT_TRUE(eid1);
        EXPECT_EQ(val1, json::Document("[1,2,3,4,5]"));
    }

    EXPECT_EQ(ledger.count_writes(dave, "client://jane", COLLECTION, "chat"), 5);
    EXPECT_EQ(ledger.count_writes(dave, "client://jon", COLLECTION, "chat"), 0);

    auto jondoc = json::Document("\"jon\"");
    ledger.put(jane, COLLECTION, "chat", jondoc, "participants.+");

    for(uint32_t i = 0; i < 3; ++i)
    {
        json::Document to_add(std::to_string(i+42));
        ledger.put(jon, COLLECTION, "chat", to_add, "messages.+");
    }

    {
        auto it2 = ledger.iterate(jane, COLLECTION, "chat", "messages");
        auto [eid, value] = it2.next();
        EXPECT_TRUE(eid);
        EXPECT_EQ(value, json::Document("[1,2,3,4,5,42,43,44]"));
    }

    EXPECT_EQ(ledger.count_writes(dave, "client://jane", COLLECTION, "chat"), 6);
    EXPECT_EQ(ledger.count_writes(dave, "client://jon", COLLECTION, "chat"), 3);
}

