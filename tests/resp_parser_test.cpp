#include "miniredis/resp/resp_parser.hpp"

#include <algorithm>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "miniredis/resp/resp_encoder.hpp"
#include "resp_test_values.hpp"

namespace miniredis::resp {
namespace {

using namespace std::string_literals;

class RespParserTest : public ::testing::Test {
protected:
    RespValue parseOne(std::string_view s) {
        std::size_t pos = 0;
        std::optional<RespValue> v = parser.parse(s, pos);
        EXPECT_TRUE(v.has_value()) << "incomplete: " << s;
        EXPECT_EQ(pos, s.size()) << "trailing bytes after value";
        return v.value_or(BulkString::null());
    }

    RespParser parser;
};

// --- Type ---

TEST_F(RespParserTest, SimpleString) {
    EXPECT_EQ(parseOne("+OK\r\n"), SimpleString("OK"));
    EXPECT_EQ(parseOne("+\r\n"), SimpleString(""));
}

TEST_F(RespParserTest, SimpleError) {
    EXPECT_EQ(parseOne("-ERR unknown command\r\n"), SimpleError("ERR unknown command"));
}

TEST_F(RespParserTest, Integers) {
    EXPECT_EQ(parseOne(":0\r\n"), Integer{0});
    EXPECT_EQ(parseOne(":-42\r\n"), Integer{-42});
    EXPECT_EQ(parseOne(":9223372036854775807\r\n"), Integer{std::numeric_limits<std::int64_t>::max()});
    EXPECT_EQ(parseOne(":-9223372036854775808\r\n"), Integer{std::numeric_limits<std::int64_t>::min()});
}

TEST_F(RespParserTest, BulkStrings) {
    EXPECT_EQ(parseOne("$5\r\nhello\r\n"), BulkString::of("hello"));
    EXPECT_EQ(parseOne("$0\r\n\r\n"), BulkString::of(""));
    EXPECT_EQ(parseOne("$-1\r\n"), BulkString::null());
}

TEST_F(RespParserTest, BulkStringIsBinarySafe) {
    const std::string payload = "\0\r\n\xff$*"s;
    ASSERT_EQ(payload.size(), 6u);
    EXPECT_EQ(parseOne("$6\r\n" + payload + "\r\n"), BulkString::of(payload));
}

TEST_F(RespParserTest, Arrays) {
    EXPECT_EQ(parseOne("*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n"),
              Array::command({"SET", "key", "value"}));
    EXPECT_EQ(parseOne("*0\r\n"), Array::of({}));
    EXPECT_EQ(parseOne("*-1\r\n"), Array::null());
}

TEST_F(RespParserTest, NestedAndMixedArrays) {
    RespValue expected = Array::of({
        Integer{1},
        Array::of({SimpleString("a"), BulkString::null()}),
        SimpleError("E"),
    });
    EXPECT_EQ(parseOne("*3\r\n:1\r\n*2\r\n+a\r\n$-1\r\n-E\r\n"), expected);
}

// --- Streaming behaviour ---

TEST_F(RespParserTest, PipelinedValuesAreReadOneAtATime) {
    const std::string buf = "+PONG\r\n:7\r\n$1\r\nx\r\n";
    std::size_t pos = 0;
    EXPECT_EQ(parser.parse(buf, pos), SimpleString("PONG"));
    EXPECT_EQ(parser.parse(buf, pos), Integer{7});
    EXPECT_EQ(parser.parse(buf, pos), BulkString::of("x"));
    EXPECT_EQ(parser.parse(buf, pos), std::nullopt);
    EXPECT_EQ(pos, buf.size());
}

TEST_F(RespParserTest, EmptyBufferIsIncomplete) {
    std::size_t pos = 0;
    EXPECT_EQ(parser.parse("", pos), std::nullopt);
}

TEST_F(RespParserTest, EveryPrefixIsIncompleteAndLeavesPositionUnchanged) {
    const std::string_view wire = "*4\r\n$3\r\nSET\r\n$-1\r\n:-12\r\n*1\r\n+nested\r\n";
    for (std::size_t len = 0; len < wire.size(); ++len) {
        std::size_t pos = 0;
        EXPECT_EQ(parser.parse(wire.substr(0, len), pos), std::nullopt) << "prefix of length " << len;
        EXPECT_EQ(pos, 0u) << "prefix of length " << len << " consumed bytes";
    }
    EXPECT_EQ(parseOne(wire).as<Array>().elements->size(), 4u);
}

TEST_F(RespParserTest, ParsesStreamDeliveredInRandomChunks) {
    std::mt19937_64 random(42);
    std::vector<RespValue> sent;
    std::string wire;
    for (int i = 0; i < 200; ++i) {
        sent.push_back(testing::randomValue(random, 3));
        encode(sent.back(), wire);
    }

    std::vector<RespValue> received;
    std::string readBuf;
    std::size_t offset = 0;
    while (offset < wire.size()) {
        const std::size_t chunk =
            std::min(wire.size() - offset, static_cast<std::size_t>(1 + testing::nextInt(random, 16)));
        readBuf.append(wire, offset, chunk);
        offset += chunk;

        std::size_t pos = 0;
        while (std::optional<RespValue> v = parser.parse(readBuf, pos)) {
            received.push_back(std::move(*v));
        }
        readBuf.erase(0, pos);
    }
    EXPECT_EQ(sent, received);
}

TEST_F(RespParserTest, HugeDeclaredArrayDoesNotPreallocate) {
    const RespParser lenient(std::numeric_limits<std::int32_t>::max(),
                             std::numeric_limits<std::int32_t>::max(), 1024, 4);
    std::size_t pos = 0;
    EXPECT_EQ(lenient.parse("*1000000000\r\n", pos), std::nullopt);
}

// --- Requests ---

TEST_F(RespParserTest, RequestAcceptsRespArrays) {
    const std::string buf = "*2\r\n$3\r\nGET\r\n$1\r\nk\r\n";
    std::size_t pos = 0;
    EXPECT_EQ(parser.parseRequest(buf, pos), Array::command({"GET", "k"}));
    EXPECT_EQ(pos, buf.size());
}

TEST_F(RespParserTest, RequestAcceptsInlineCommands) {
    const std::string buf = "SET  k\tv\r\nPING\n";
    std::size_t pos = 0;
    EXPECT_EQ(parser.parseRequest(buf, pos), Array::command({"SET", "k", "v"}));
    EXPECT_EQ(parser.parseRequest(buf, pos), Array::command({"PING"}));
    EXPECT_EQ(pos, buf.size());
    EXPECT_EQ(parser.parseRequest(buf, pos), std::nullopt);
}

TEST_F(RespParserTest, RequestSkipsBlankLinesAndEmptyArrays) {
    const std::string buf = "\r\n  \n*0\r\nPING\r\n";
    std::size_t pos = 0;
    EXPECT_EQ(parser.parseRequest(buf, pos), Array::command({"PING"}));
    EXPECT_EQ(pos, buf.size());
}

TEST_F(RespParserTest, IncompleteInlineRequestWaitsForNewline) {
    std::size_t pos = 0;
    EXPECT_EQ(parser.parseRequest("PIN", pos), std::nullopt);
    EXPECT_EQ(pos, 0u);
}

TEST_F(RespParserTest, InlineRequestLengthIsBounded) {
    const RespParser small(4, 2, 8, 4);
    std::size_t pos = 0;
    EXPECT_THROW(small.parseRequest(std::string(100, 'x'), pos), RespProtocolError);
}

// --- Incorrect input ---

class RespParserMalformedTest : public ::testing::TestWithParam<std::string> {};

TEST_P(RespParserMalformedTest, RejectsMalformedInput) {
    RespParser parser;
    std::size_t pos = 0;
    EXPECT_THROW(parser.parse(GetParam(), pos), RespProtocolError);
}

INSTANTIATE_TEST_SUITE_P(Malformed, RespParserMalformedTest,
                         ::testing::Values("?foo\r\n",
                                           "+OK\rX",
                                           "+OK\n",
                                           ":\r\n",
                                           ":-\r\n",
                                           ":12a\r\n",
                                           ": 1\r\n",
                                           ":+1\r\n",
                                           ":9223372036854775808\r\n",
                                           ":-9223372036854775809\r\n",
                                           "$-2\r\n",
                                           "$abc\r\n",
                                           "$3\r\nabcde\r\n",
                                           "$3\r\nab\r\n\r\n",
                                           "*-5\r\n",
                                           "*1\r\n?\r\n"));

TEST_F(RespParserTest, EnforcesBulkLengthLimit) {
    const RespParser small(4, 8, 64, 4);
    std::size_t pos = 0;
    EXPECT_EQ(small.parse("$4\r\nabcd\r\n", pos), BulkString::of("abcd"));
    pos = 0;
    EXPECT_THROW(small.parse("$5\r\n", pos), RespProtocolError);
}

TEST_F(RespParserTest, EnforcesArrayLengthLimit) {
    const RespParser small(4, 2, 64, 4);
    std::size_t pos = 0;
    EXPECT_THROW(small.parse("*3\r\n", pos), RespProtocolError);
}

TEST_F(RespParserTest, EnforcesLineLengthLimitBeforeCrlfArrives) {
    const RespParser small(4, 2, 8, 4);
    std::size_t pos = 0;
    EXPECT_THROW(small.parse("+" + std::string(100, 'x'), pos), RespProtocolError);
}

TEST_F(RespParserTest, EnforcesNestingDepthLimit) {
    const RespParser shallow(4, 2, 64, 3);
    std::size_t pos = 0;
    EXPECT_EQ(shallow.parse("*1\r\n*1\r\n*0\r\n", pos), Array::of({Array::of({Array::of({})})}));
    pos = 0;
    EXPECT_THROW(shallow.parse("*1\r\n*1\r\n*1\r\n*0\r\n", pos), RespProtocolError);
}

TEST_F(RespParserTest, DeeplyNestedInputFailsCleanlyWithDefaultLimits) {
    std::string bomb;
    for (int i = 0; i < 100'000; ++i) {
        bomb += "*1\r\n";
    }
    std::size_t pos = 0;
    EXPECT_THROW(parser.parse(bomb, pos), RespProtocolError);
}

TEST(RespParserLimitsTest, RejectsInvalidLimits) {
    EXPECT_THROW(RespParser(-1, 1, 1, 1), std::invalid_argument);
    EXPECT_THROW(RespParser(1, -1, 1, 1), std::invalid_argument);
    EXPECT_THROW(RespParser(1, 1, 0, 1), std::invalid_argument);
    EXPECT_THROW(RespParser(1, 1, 1, 0), std::invalid_argument);
}

TEST(RespParserFuzzTest, RandomBytesNeverCrash) {
    std::mt19937_64 random(7);
    const std::string_view alphabet = "+-:$*\r\n0123456789-abc";
    const RespParser small(64, 16, 64, 8);

    for (int iter = 0; iter < 50'000; ++iter) {
        std::string input(static_cast<std::size_t>(testing::nextInt(random, 40)), '\0');
        for (char& c : input) {
            c = testing::nextInt(random, 2) == 0
                    ? alphabet[static_cast<std::size_t>(testing::nextInt(random, static_cast<int>(alphabet.size())))]
                    : static_cast<char>(testing::nextInt(random, 256));
        }
        std::size_t pos = 0;
        try {
            if (small.parse(input, pos)) {
                EXPECT_GT(pos, 0u);
            } else {
                EXPECT_EQ(pos, 0u);
            }
        } catch (const RespProtocolError&) {
        }
    }
}

}  // namespace
}  // namespace miniredis::resp
