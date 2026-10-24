#include "miniredis/resp/resp_encoder.hpp"

#include <string_view>

#include "overloaded.hpp"

namespace miniredis::resp {
namespace {

constexpr std::string_view kCrlf = "\r\n";

void writeLine(std::string& out, char type, std::string_view content) {
    out += type;
    out += content;
    out += kCrlf;
}

}  // namespace

std::string encode(const RespValue& value) {
    std::string out;
    encode(value, out);
    return out;
}

void encode(const RespValue& value, std::string& out) {
    std::visit(detail::Overloaded{
                   [&](const SimpleString& s) { writeLine(out, '+', s.value()); },
                   [&](const SimpleError& e) { writeLine(out, '-', e.message()); },
                   [&](const Integer& i) { writeLine(out, ':', std::to_string(i.value)); },
                   [&](const BulkString& b) {
                       if (b.isNull()) {
                           writeLine(out, '$', "-1");
                           return;
                       }
                       writeLine(out, '$', std::to_string(b.data->size()));
                       out += *b.data;
                       out += kCrlf;
                   },
                   [&](const Array& a) {
                       if (a.isNull()) {
                           writeLine(out, '*', "-1");
                           return;
                       }
                       writeLine(out, '*', std::to_string(a.elements->size()));
                       for (const RespValue& element : *a.elements) {
                           encode(element, out);
                       }
                   },
               },
               value.value);
}

}  // namespace miniredis::resp
