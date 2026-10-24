#pragma once

#include <string>

#include "miniredis/resp/resp_value.hpp"

namespace miniredis::resp {

std::string encode(const RespValue& value);

void encode(const RespValue& value, std::string& out);

}  // namespace miniredis::resp
