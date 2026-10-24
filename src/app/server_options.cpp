#include "miniredis/app/server_options.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace miniredis::app {
namespace {

long long parsePositive(std::string_view flag, std::string_view text, long long max) {
    long long value = 0;
    auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc() || end != text.data() + text.size() || value <= 0 || value > max) {
        throw std::invalid_argument(std::string(flag) + " expects an integer from 1 to " +
                                    std::to_string(max) + ", got '" + std::string(text) + "'");
    }
    return value;
}

}  // namespace

std::string usage(std::string_view program) {
    return "usage: " + std::string(program) +
           " [--bind ADDR] [--port N] [--threads N] [--capacity N] [--reaper-interval-ms N]\n"
           "  --bind ADDR             IPv4 address\n"
           "  --port N                TCP port\n"
           "  --threads N             worker threads executing requests\n"
           "  --capacity N            max keys before LRU eviction\n"
           "  --reaper-interval-ms N  key purge rate\n";
}

ServerOptions parseOptions(std::span<const std::string_view> args) {
    ServerOptions options;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view flag = args[i];
        if (flag == "--help" || flag == "-h") {
            options.showHelp = true;
            continue;
        }
        if (i + 1 >= args.size()) {
            throw std::invalid_argument("unknown flag or missing value: " + std::string(flag));
        }
        const std::string_view value = args[++i];

        if (flag == "--bind") {
            options.server.bindAddress = std::string(value);
        } else if (flag == "--port") {
            options.server.port =
                static_cast<std::uint16_t>(parsePositive(flag, value, std::numeric_limits<std::uint16_t>::max()));
        } else if (flag == "--threads") {
            options.server.workerThreads = static_cast<std::size_t>(parsePositive(flag, value, 1024));
        } else if (flag == "--capacity") {
            options.capacity = parsePositive(flag, value, std::numeric_limits<long long>::max());
        } else if (flag == "--reaper-interval-ms") {
            options.reaperInterval = std::chrono::milliseconds(parsePositive(flag, value, 60'000));
        } else {
            throw std::invalid_argument("unknown flag: " + std::string(flag));
        }
    }
    return options;
}

}  // namespace miniredis::app
