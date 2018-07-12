
#include <gtest/gtest.h>

#include "../src/enclave/MultiMap.h"
#include "../src/enclave/logging.h"
#include "../src/enclave/Index.h"
#include "../src/server/Disk.h"
#include "../src/enclave/version_number.h"

#include "credb/base64.h"

#include "credb/event_id.h"

namespace credb
{

class PrimitivesTest : public testing::Test
{
};

TEST(PrimitivesTest, base64)
{
    std::string input = "thisisatest";

    auto enc = base64_encode(reinterpret_cast<const uint8_t*>(input.c_str()), input.size());
    auto output = base64_decode(enc);

    EXPECT_EQ(input, output);
}

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

    block_index_t index_a = 18;
    block_index_t index_b = 242;

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

TEST(PrimitivesTest, version_number)
{
    using trusted::version_number;

    EXPECT_TRUE(version_number(60000) < version_number(5));
    EXPECT_TRUE(version_number(50) < version_number(1000));
    EXPECT_FALSE(version_number(5) < version_number(4));
}

}
