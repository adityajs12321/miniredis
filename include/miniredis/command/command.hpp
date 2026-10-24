#pragma once

#include <span>
#include <string>
#include <string_view>

#include "miniredis/resp/resp_value.hpp"
#include "miniredis/store/key_value_store.hpp"

namespace miniredis::command {

using Args = std::span<const std::string>;

class Command {
public:
    virtual ~Command() = default;

    virtual std::string_view name() const = 0;

    virtual int arity() const = 0;

    virtual resp::RespValue execute(Args args, store::KeyValueStore& store) const = 0;
};

resp::SimpleError errorReply(std::string message);

}  // namespace miniredis::command
