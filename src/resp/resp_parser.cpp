#include "miniredis/resp/resp_parser.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace miniredis::resp {
namespace {

constexpr std::int64_t kMaxInitialArrayCapacity = 1024;

std::int64_t parseInteger(std::string_view text) {
    if (text.empty()) {
        throw RespProtocolError("empty integer");
    }
    if (text == "-") {
        throw RespProtocolError("integer has no digits");
    }
    std::int64_t result = 0;
    auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), result);
    if (ec == std::errc::result_out_of_range) {
        throw RespProtocolError("integer overflow '" + std::string(text) + "'");
    }
    if (ec != std::errc() || end != text.data() + text.size()) {
        throw RespProtocolError("invalid integer '" + std::string(text) + "'");
    }
    return result;
}

std::string hexByte(char c) {
    char out[3];
    std::snprintf(out, sizeof out, "%02x", static_cast<unsigned char>(c));
    return out;
}

}  // namespace

// --- Construction ---

RespParser::RespParser()
    : RespParser(kDefaultMaxBulkLength, kDefaultMaxArrayLength,
                 static_cast<std::int64_t>(kDefaultMaxLineLength), kDefaultMaxDepth) {}

RespParser::RespParser(std::int64_t maxBulkLength, std::int64_t maxArrayLength,
                       std::int64_t maxLineLength, int maxDepth)
    : maxBulkLength_(maxBulkLength),
      maxArrayLength_(maxArrayLength),
      maxLineLength_(static_cast<std::size_t>(maxLineLength)),
      maxDepth_(maxDepth) {
    if (maxBulkLength < 0 || maxArrayLength < 0 || maxLineLength <= 0 || maxDepth <= 0) {
        throw std::invalid_argument("limits must be positive");
    }
}

// --- Public entry points ---

std::optional<RespValue> RespParser::parse(std::string_view buf, std::size_t& pos) const {
    std::size_t cursor = pos;
    std::optional<RespValue> value = parseValue(buf, cursor, 0);
    if (value) {
        pos = cursor;
    }
    return value;
}

std::optional<RespValue> RespParser::parseRequest(std::string_view buf, std::size_t& pos) const {
    std::size_t cursor = pos;
    while (cursor < buf.size()) {
        std::optional<RespValue> request =
            buf[cursor] == '*' ? parseValue(buf, cursor, 0) : parseInline(buf, cursor);
        if (!request) {
            return std::nullopt;
        }
        pos = cursor;
        const Array& array = request->as<Array>();
        if (!array.isNull() && !array.elements->empty()) {
            return request;
        }
    }
    return std::nullopt;
}

// --- Inline commands ---

std::optional<RespValue> RespParser::parseInline(std::string_view buf, std::size_t& pos) const {
    const std::size_t lf = buf.find('\n', pos);
    const std::size_t lineEnd = lf == std::string_view::npos ? buf.size() : lf;
    if (lineEnd - pos > maxLineLength_) {
        throw RespProtocolError("inline request longer than " + std::to_string(maxLineLength_) + " bytes");
    }
    if (lf == std::string_view::npos) {
        return std::nullopt;
    }

    std::string_view line = buf.substr(pos, lf - pos);
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
    }
    pos = lf + 1;

    // Split on spaces and tabs
    std::vector<RespValue> args;
    std::size_t i = 0;
    while (i < line.size()) {
        if (line[i] == ' ' || line[i] == '\t') {
            ++i;
            continue;
        }
        const std::size_t end = std::min(line.find_first_of(" \t", i), line.size());
        args.emplace_back(BulkString::of(std::string(line.substr(i, end - i))));
        i = end;
    }
    return Array{std::move(args)};
}

// --- RESP types ---

std::optional<RespValue> RespParser::parseValue(std::string_view buf, std::size_t& pos,
                                                int depth) const {
    if (pos >= buf.size()) {
        return std::nullopt;
    }
    const char type = buf[pos++];
    if (type == '$') {
        return parseBulkString(buf, pos);
    }
    if (type == '*') {
        return parseArray(buf, pos, depth);
    }

    // Single-line types: simple string, error, integer
    const std::size_t typeOffset = pos - 1;
    std::optional<std::string_view> line = readLine(buf, pos);
    if (!line) {
        return std::nullopt;
    }
    switch (type) {
        case '+':
            return SimpleString(std::string(*line));
        case '-':
            return SimpleError(std::string(*line));
        case ':':
            return Integer{parseInteger(*line)};
        default:
            throw RespProtocolError("unknown type byte 0x" + hexByte(type) + " at offset " +
                                    std::to_string(typeOffset));
    }
}

std::optional<RespValue> RespParser::parseBulkString(std::string_view buf, std::size_t& pos) const {
    std::optional<std::string_view> line = readLine(buf, pos);
    if (!line) {
        return std::nullopt;
    }
    const std::int64_t length = parseInteger(*line);
    if (length == -1) {
        return BulkString::null();
    }
    if (length < 0 || length > maxBulkLength_) {
        throw RespProtocolError("invalid bulk length " + std::to_string(length));
    }
    const auto size = static_cast<std::size_t>(length);
    if (buf.size() - pos < size + 2) {
        return std::nullopt;
    }

    std::string data(buf.substr(pos, size));
    pos += size;
    if (buf[pos] != '\r' || buf[pos + 1] != '\n') {
        throw RespProtocolError("bulk string of length " + std::to_string(length) +
                                " not terminated by CRLF");
    }
    pos += 2;
    return BulkString::of(std::move(data));
}

std::optional<RespValue> RespParser::parseArray(std::string_view buf, std::size_t& pos,
                                                int depth) const {
    std::optional<std::string_view> line = readLine(buf, pos);
    if (!line) {
        return std::nullopt;
    }
    const std::int64_t length = parseInteger(*line);
    if (length == -1) {
        return Array::null();
    }
    if (length < 0 || length > maxArrayLength_) {
        throw RespProtocolError("invalid array length " + std::to_string(length));
    }
    if (depth + 1 > maxDepth_) {
        throw RespProtocolError("arrays nested deeper than " + std::to_string(maxDepth_));
    }

    std::vector<RespValue> elements;
    elements.reserve(static_cast<std::size_t>(std::min(length, kMaxInitialArrayCapacity)));
    for (std::int64_t i = 0; i < length; ++i) {
        std::optional<RespValue> element = parseValue(buf, pos, depth + 1);
        if (!element) {
            return std::nullopt;
        }
        elements.push_back(std::move(*element));
    }
    return Array{std::move(elements)};
}

// --- Line reading ---

std::optional<std::string_view> RespParser::readLine(std::string_view buf, std::size_t& pos) const {
    const std::size_t start = pos;
    for (std::size_t i = start; i < buf.size(); ++i) {
        const char b = buf[i];
        if (b == '\r') {
            if (i + 1 == buf.size()) {
                return std::nullopt;
            }
            if (buf[i + 1] != '\n') {
                throw RespProtocolError("CR not followed by LF at offset " + std::to_string(i));
            }
            pos = i + 2;
            return buf.substr(start, i - start);
        }
        if (b == '\n') {
            throw RespProtocolError("LF without preceding CR at offset " + std::to_string(i));
        }
        if (i - start >= maxLineLength_) {
            throw RespProtocolError("line longer than " + std::to_string(maxLineLength_) + " bytes");
        }
    }
    return std::nullopt;
}

}  // namespace miniredis::resp
