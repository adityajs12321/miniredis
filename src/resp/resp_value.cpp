#include "miniredis/resp/resp_value.hpp"

#include <ostream>
#include <stdexcept>

#include "overloaded.hpp"

namespace miniredis::resp {
namespace {

const std::string& requireNoLineBreaks(const std::string& s) {
    if (s.find_first_of("\r\n") != std::string::npos) {
        throw std::invalid_argument("simple strings and errors cannot contain CR or LF");
    }
    return s;
}

}  // namespace

SimpleString::SimpleString(std::string value) : value_(std::move(value)) {
    requireNoLineBreaks(value_);
}

SimpleError::SimpleError(std::string message) : message_(std::move(message)) {
    requireNoLineBreaks(message_);
}

Array Array::of(std::initializer_list<RespValue> elements) {
    return Array{std::vector<RespValue>(elements)};
}

Array Array::command(std::initializer_list<std::string_view> args) {
    std::vector<RespValue> elements;
    elements.reserve(args.size());
    for (std::string_view arg : args) {
        elements.emplace_back(BulkString::of(std::string(arg)));
    }
    return Array{std::move(elements)};
}

bool Array::operator==(const Array& other) const {
    return elements == other.elements;
}

// --- Debug ---

std::ostream& operator<<(std::ostream& os, const RespValue& v) {
    std::visit(detail::Overloaded{
                   [&](const SimpleString& s) { os << "SimpleString[" << s.value() << "]"; },
                   [&](const SimpleError& e) { os << "SimpleError[" << e.message() << "]"; },
                   [&](const Integer& i) { os << "Integer[" << i.value << "]"; },
                   [&](const BulkString& b) {
                       if (b.isNull()) {
                           os << "BulkString[null]";
                       } else {
                           os << "BulkString[" << *b.data << "]";
                       }
                   },
                   [&](const Array& a) {
                       if (a.isNull()) {
                           os << "Array[null]";
                           return;
                       }
                       os << "Array[";
                       const char* sep = "";
                       for (const RespValue& e : *a.elements) {
                           os << sep << e;
                           sep = ", ";
                       }
                       os << "]";
                   },
               },
               v.value);
    return os;
}

}  // namespace miniredis::resp
