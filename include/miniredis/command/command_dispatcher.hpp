#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "miniredis/command/command.hpp"
#include "miniredis/resp/resp_value.hpp"
#include "miniredis/store/key_value_store.hpp"

namespace miniredis::command {

class CommandDispatcher {
public:
    explicit CommandDispatcher(store::KeyValueStore& store);

    void registerCommand(std::unique_ptr<Command> command);

    resp::RespValue dispatch(const resp::RespValue& request) const;

private:
    store::KeyValueStore& store_;
    std::unordered_map<std::string, std::unique_ptr<Command>> commands_;
};

void registerBuiltinCommands(CommandDispatcher& dispatcher);

}  // namespace miniredis::command
