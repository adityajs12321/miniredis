#pragma once

#include <chrono>
#include <span>
#include <string>
#include <string_view>

#include "miniredis/net/tcp_server.hpp"

namespace miniredis::app {

struct ServerOptions {
    net::ServerConfig server;
    long long capacity = 1'000'000;
    std::chrono::milliseconds reaperInterval{100};
    bool showHelp = false;
};

std::string usage(std::string_view program);

ServerOptions parseOptions(std::span<const std::string_view> args);

}  // namespace miniredis::app
