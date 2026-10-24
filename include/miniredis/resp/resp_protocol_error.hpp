#pragma once

#include <stdexcept>
#include <string>

namespace miniredis::resp {

class RespProtocolError : public std::runtime_error {
public:
    explicit RespProtocolError(const std::string& message) : std::runtime_error(message) {}
};

}  // namespace miniredis::resp
