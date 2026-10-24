#include "miniredis/app/server_options.hpp"

#include <initializer_list>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace miniredis::app {
namespace {

using namespace std::chrono_literals;

ServerOptions parse(std::initializer_list<std::string_view> args) {
    const std::vector<std::string_view> v(args);
    return parseOptions(v);
}

TEST(ServerOptionsTest, Defaults) {
    ServerOptions options = parse({});
    EXPECT_EQ(options.server.bindAddress, "127.0.0.1");
    EXPECT_EQ(options.server.port, 6379);
    EXPECT_EQ(options.server.workerThreads, 8u);
    EXPECT_EQ(options.capacity, 1'000'000);
    EXPECT_EQ(options.reaperInterval, 100ms);
    EXPECT_FALSE(options.showHelp);
}

TEST(ServerOptionsTest, ParsesEveryFlag) {
    ServerOptions options = parse({"--bind", "0.0.0.0", "--port", "7000", "--threads", "16", "--capacity",
                                   "500", "--reaper-interval-ms", "250"});
    EXPECT_EQ(options.server.bindAddress, "0.0.0.0");
    EXPECT_EQ(options.server.port, 7000);
    EXPECT_EQ(options.server.workerThreads, 16u);
    EXPECT_EQ(options.capacity, 500);
    EXPECT_EQ(options.reaperInterval, 250ms);
}

TEST(ServerOptionsTest, Help) {
    EXPECT_TRUE(parse({"--help"}).showHelp);
    EXPECT_TRUE(parse({"-h"}).showHelp);
}

TEST(ServerOptionsTest, RejectsBadInput) {
    EXPECT_THROW(parse({"--port"}), std::invalid_argument);
    EXPECT_THROW(parse({"--port", "0"}), std::invalid_argument);
    EXPECT_THROW(parse({"--port", "65536"}), std::invalid_argument);
    EXPECT_THROW(parse({"--port", "12ab"}), std::invalid_argument);
    EXPECT_THROW(parse({"--threads", "-1"}), std::invalid_argument);
    EXPECT_THROW(parse({"--capacity", "0"}), std::invalid_argument);
    EXPECT_THROW(parse({"--verbose", "yes"}), std::invalid_argument);
}

}  // namespace
}  // namespace miniredis::app
