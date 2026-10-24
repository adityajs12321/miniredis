#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "miniredis/resp/resp_protocol_error.hpp"
#include "miniredis/resp/resp_value.hpp"

namespace miniredis::resp {

class RespParser {
public:
    static constexpr std::int64_t kDefaultMaxBulkLength = 512LL * 1024 * 1024;
    static constexpr std::int64_t kDefaultMaxArrayLength = 1024 * 1024;
    static constexpr std::size_t kDefaultMaxLineLength = 64 * 1024;
    static constexpr int kDefaultMaxDepth = 32;

    RespParser();

    RespParser(std::int64_t maxBulkLength, std::int64_t maxArrayLength, std::int64_t maxLineLength,
               int maxDepth);

    std::optional<RespValue> parse(std::string_view buf, std::size_t& pos) const;

    std::optional<RespValue> parseRequest(std::string_view buf, std::size_t& pos) const;

private:
    std::optional<RespValue> parseInline(std::string_view buf, std::size_t& pos) const;
    std::optional<RespValue> parseValue(std::string_view buf, std::size_t& pos, int depth) const;
    std::optional<RespValue> parseBulkString(std::string_view buf, std::size_t& pos) const;
    std::optional<RespValue> parseArray(std::string_view buf, std::size_t& pos, int depth) const;
    std::optional<std::string_view> readLine(std::string_view buf, std::size_t& pos) const;

    std::int64_t maxBulkLength_;
    std::int64_t maxArrayLength_;
    std::size_t maxLineLength_;
    int maxDepth_;
};

}  // namespace miniredis::resp
