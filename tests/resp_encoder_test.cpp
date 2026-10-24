#include "miniredis/resp/resp_encoder.hpp"

#include <random>
#include <stdexcept>

#include <gtest/gtest.h>

#include "miniredis/resp/resp_parser.hpp"
#include "resp_test_values.hpp"

namespace miniredis::resp {
namespace {

TEST(RespEncoderTest, EncodesEachType) {
    EXPECT_EQ(encode(SimpleString("OK")), "+OK\r\n");
    EXPECT_EQ(encode(SimpleError("ERR bad")), "-ERR bad\r\n");
    EXPECT_EQ(encode(Integer{-9}), ":-9\r\n");
    EXPECT_EQ(encode(BulkString::of("hello")), "$5\r\nhello\r\n");
    EXPECT_EQ(encode(BulkString::of("")), "$0\r\n\r\n");
    EXPECT_EQ(encode(BulkString::null()), "$-1\r\n");
    EXPECT_EQ(encode(Array::of({})), "*0\r\n");
    EXPECT_EQ(encode(Array::null()), "*-1\r\n");
    EXPECT_EQ(encode(Array::command({"GET", "k"})), "*2\r\n$3\r\nGET\r\n$1\r\nk\r\n");
}

TEST(RespEncoderTest, BulkLengthCountsBytesNotChars) {
    EXPECT_EQ(encode(BulkString::of("é")), "$2\r\né\r\n");
}

TEST(RespEncoderTest, RejectsLineBreaksInSimpleTypes) {
    EXPECT_THROW(SimpleString("OK\r\n+INJECTED"), std::invalid_argument);
    EXPECT_THROW(SimpleError("ERR\n"), std::invalid_argument);
}

TEST(RespEncoderTest, RandomValuesRoundTrip) {
    std::mt19937_64 random(1);
    RespParser parser;
    for (int i = 0; i < 5'000; ++i) {
        RespValue original = testing::randomValue(random, 4);
        std::string wire = encode(original);
        std::size_t pos = 0;
        EXPECT_EQ(parser.parse(wire, pos), original);
        EXPECT_EQ(pos, wire.size());
    }
}

}  // namespace
}  // namespace miniredis::resp
