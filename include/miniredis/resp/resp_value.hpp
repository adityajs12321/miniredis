#pragma once

#include <cstdint>
#include <initializer_list>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace miniredis::resp {

struct RespValue;

class SimpleString {
public:
    explicit SimpleString(std::string value);
    const std::string& value() const noexcept { return value_; }
    bool operator==(const SimpleString&) const = default;

private:
    std::string value_;
};

class SimpleError {
public:
    explicit SimpleError(std::string message);
    const std::string& message() const noexcept { return message_; }
    bool operator==(const SimpleError&) const = default;

private:
    std::string message_;
};

struct Integer {
    std::int64_t value;
    bool operator==(const Integer&) const = default;
};

struct BulkString {
    std::optional<std::string> data;

    static BulkString of(std::string bytes) { return BulkString{std::move(bytes)}; }
    static BulkString null() { return BulkString{std::nullopt}; }

    bool isNull() const noexcept { return !data.has_value(); }
    bool operator==(const BulkString&) const = default;
};

struct Array {
    std::optional<std::vector<RespValue>> elements;

    Array(std::optional<std::vector<RespValue>> elements);
    Array(const Array&);
    Array(Array&&) noexcept;
    Array& operator=(const Array&);
    Array& operator=(Array&&) noexcept;
    ~Array();

    static Array of(std::initializer_list<RespValue> elements);
    static Array null();

    static Array command(std::initializer_list<std::string_view> args);

    bool isNull() const noexcept { return !elements.has_value(); }
    bool operator==(const Array&) const;
};

struct RespValue {
    using Variant = std::variant<SimpleString, SimpleError, Integer, BulkString, Array>;

    Variant value;

    template <typename T>
        requires std::is_constructible_v<Variant, T&&>
    RespValue(T&& v) : value(std::forward<T>(v)) {}

    template <typename T>
    bool is() const noexcept { return std::holds_alternative<T>(value); }

    template <typename T>
    const T& as() const { return std::get<T>(value); }

    bool operator==(const RespValue&) const = default;
};

// Array members defined here because they need RespValue to be complete

inline Array::Array(std::optional<std::vector<RespValue>> elements) : elements(std::move(elements)) {}
inline Array::Array(const Array&) = default;
inline Array::Array(Array&&) noexcept = default;
inline Array& Array::operator=(const Array&) = default;
inline Array& Array::operator=(Array&&) noexcept = default;
inline Array::~Array() = default;
inline Array Array::null() { return Array{std::nullopt}; }

std::ostream& operator<<(std::ostream& os, const RespValue& v);

}  // namespace miniredis::resp
