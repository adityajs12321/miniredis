#include "miniredis/command/command_dispatcher.hpp"

#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string_view>

#include <gtest/gtest.h>

#include "manual_clock.hpp"

namespace miniredis::command {
namespace {

using namespace std::chrono_literals;
using resp::Array;
using resp::BulkString;
using resp::Integer;
using resp::RespValue;
using resp::SimpleError;
using resp::SimpleString;

class CommandDispatcherTest : public ::testing::Test {
protected:
    CommandDispatcherTest() { registerBuiltinCommands(dispatcher); }

    RespValue run(std::initializer_list<std::string_view> args) {
        return dispatcher.dispatch(Array::command(args));
    }

    miniredis::testing::ManualClock clock;
    store::KeyValueStore store{100, clock};
    CommandDispatcher dispatcher{store};
};

// --- Built-in commands ---

TEST_F(CommandDispatcherTest, Ping) {
    EXPECT_EQ(run({"PING"}), SimpleString("PONG"));
    EXPECT_EQ(run({"ping", "hi"}), BulkString::of("hi"));
    EXPECT_EQ(run({"PING", "a", "b"}), SimpleError("ERR wrong number of arguments for 'ping' command"));
}

TEST_F(CommandDispatcherTest, SetAndGet) {
    EXPECT_EQ(run({"SET", "k", "hello world"}), SimpleString("OK"));
    EXPECT_EQ(run({"GET", "k"}), BulkString::of("hello world"));
    EXPECT_EQ(run({"GET", "missing"}), BulkString::null());
}

TEST_F(CommandDispatcherTest, CommandNamesAreCaseInsensitive) {
    EXPECT_EQ(run({"sEt", "k", "v"}), SimpleString("OK"));
    EXPECT_EQ(run({"get", "k"}), BulkString::of("v"));
}

TEST_F(CommandDispatcherTest, ValuesAreBinarySafe) {
    const std::string binary("a\0b\r\nc", 6);
    EXPECT_EQ(run({"SET", "k", binary}), SimpleString("OK"));
    EXPECT_EQ(run({"GET", "k"}), BulkString::of(binary));
}

TEST_F(CommandDispatcherTest, SetWithExAndPx) {
    run({"SET", "a", "v", "EX", "10"});
    run({"SET", "b", "v", "px", "1500"});
    EXPECT_EQ(run({"TTL", "a"}), Integer{10});
    EXPECT_EQ(run({"TTL", "b"}), Integer{2});
    clock.advance(1500ms);
    EXPECT_EQ(run({"GET", "b"}), BulkString::null());
    EXPECT_EQ(run({"GET", "a"}), BulkString::of("v"));
}

TEST_F(CommandDispatcherTest, SetRejectsBadOptions) {
    EXPECT_EQ(run({"SET", "k", "v", "EX"}), SimpleError("ERR syntax error"));
    EXPECT_EQ(run({"SET", "k", "v", "NX", "1"}), SimpleError("ERR syntax error"));
    EXPECT_EQ(run({"SET", "k", "v", "EX", "1", "PX", "1"}), SimpleError("ERR syntax error"));
    EXPECT_EQ(run({"SET", "k", "v", "EX", "abc"}), SimpleError("ERR value is not an integer or out of range"));
    EXPECT_EQ(run({"SET", "k", "v", "EX", "0"}), SimpleError("ERR invalid expire time in 'set' command"));
    EXPECT_EQ(run({"SET", "k", "v", "EX", "-1"}), SimpleError("ERR invalid expire time in 'set' command"));
    EXPECT_EQ(run({"SET", "k", "v", "EX", "9223372036854775807"}),
              SimpleError("ERR invalid expire time in 'set' command"));
    EXPECT_EQ(run({"GET", "k"}), BulkString::null());
}

TEST_F(CommandDispatcherTest, DelCountsDeletedKeys) {
    run({"SET", "a", "1"});
    run({"SET", "b", "2"});
    EXPECT_EQ(run({"DEL", "a", "b", "c"}), Integer{2});
    EXPECT_EQ(run({"DEL", "a"}), Integer{0});
}

TEST_F(CommandDispatcherTest, ExpireAndTtl) {
    EXPECT_EQ(run({"TTL", "k"}), Integer{-2});
    EXPECT_EQ(run({"EXPIRE", "k", "10"}), Integer{0});

    run({"SET", "k", "v"});
    EXPECT_EQ(run({"TTL", "k"}), Integer{-1});
    EXPECT_EQ(run({"EXPIRE", "k", "10"}), Integer{1});
    EXPECT_EQ(run({"TTL", "k"}), Integer{10});

    clock.advance(10s);
    EXPECT_EQ(run({"GET", "k"}), BulkString::null());
    EXPECT_EQ(run({"TTL", "k"}), Integer{-2});
}

TEST_F(CommandDispatcherTest, NonPositiveExpireDeletes) {
    run({"SET", "k", "v"});
    EXPECT_EQ(run({"EXPIRE", "k", "-1"}), Integer{1});
    EXPECT_EQ(run({"GET", "k"}), BulkString::null());
}

TEST_F(CommandDispatcherTest, ExpireRejectsBadSeconds) {
    run({"SET", "k", "v"});
    EXPECT_EQ(run({"EXPIRE", "k", "soon"}), SimpleError("ERR value is not an integer or out of range"));
    EXPECT_EQ(run({"EXPIRE", "k", "9223372036854775807"}),
              SimpleError("ERR invalid expire time in 'expire' command"));
    EXPECT_EQ(run({"TTL", "k"}), Integer{-1});
}

TEST_F(CommandDispatcherTest, SetClearsTtl) {
    run({"SET", "k", "v", "EX", "10"});
    run({"SET", "k", "v2"});
    EXPECT_EQ(run({"TTL", "k"}), Integer{-1});
}

// --- Dispatch and validation ---

TEST_F(CommandDispatcherTest, ArityIsEnforced) {
    EXPECT_EQ(run({"GET"}), SimpleError("ERR wrong number of arguments for 'get' command"));
    EXPECT_EQ(run({"GET", "a", "b"}), SimpleError("ERR wrong number of arguments for 'get' command"));
    EXPECT_EQ(run({"SET", "k"}), SimpleError("ERR wrong number of arguments for 'set' command"));
    EXPECT_EQ(run({"DEL"}), SimpleError("ERR wrong number of arguments for 'del' command"));
    EXPECT_EQ(run({"EXPIRE", "k"}), SimpleError("ERR wrong number of arguments for 'expire' command"));
    EXPECT_EQ(run({"TTL"}), SimpleError("ERR wrong number of arguments for 'ttl' command"));
}

TEST_F(CommandDispatcherTest, UnknownCommand) {
    EXPECT_EQ(run({"FOO", "bar"}), SimpleError("ERR unknown command 'FOO'"));
}

TEST_F(CommandDispatcherTest, ClientTextCannotInjectFrames) {
    RespValue reply = run({"BAD\r\n+OK"});
    EXPECT_EQ(reply, SimpleError("ERR unknown command 'BAD  +OK'"));
}

TEST_F(CommandDispatcherTest, RejectsRequestsThatAreNotArraysOfBulkStrings) {
    const SimpleError expected("ERR Protocol error: expected a non-empty array of bulk strings");
    EXPECT_EQ(dispatcher.dispatch(SimpleString("PING")), expected);
    EXPECT_EQ(dispatcher.dispatch(Array::of({})), expected);
    EXPECT_EQ(dispatcher.dispatch(Array::null()), expected);
    EXPECT_EQ(dispatcher.dispatch(Array::of({BulkString::of("GET"), Integer{1}})), expected);
    EXPECT_EQ(dispatcher.dispatch(Array::of({BulkString::of("GET"), BulkString::null()})), expected);
}

TEST_F(CommandDispatcherTest, DuplicateRegistrationThrows) {
    class FakePing final : public Command {
    public:
        std::string_view name() const override { return "PING"; }
        int arity() const override { return 1; }
        RespValue execute(Args, store::KeyValueStore&) const override { return SimpleString("x"); }
    };
    EXPECT_THROW(dispatcher.registerCommand(std::make_unique<FakePing>()), std::invalid_argument);
}

}  // namespace
}  // namespace miniredis::command
