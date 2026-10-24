#include "miniredis/command/command_dispatcher.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <vector>

namespace miniredis::command {
namespace {

std::string toLower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool arityMatches(int arity, std::size_t argc) {
    const auto required = static_cast<std::size_t>(arity < 0 ? -arity : arity);
    return arity < 0 ? argc >= required : argc == required;
}

}  // namespace

resp::SimpleError errorReply(std::string message) {
    std::replace_if(message.begin(), message.end(), [](char c) { return c == '\r' || c == '\n'; }, ' ');
    return resp::SimpleError(std::move(message));
}

CommandDispatcher::CommandDispatcher(store::KeyValueStore& store) : store_(store) {}

void CommandDispatcher::registerCommand(std::unique_ptr<Command> command) {
    std::string name = toLower(command->name());
    if (!commands_.emplace(name, std::move(command)).second) {
        throw std::invalid_argument("command already registered: " + name);
    }
}

resp::RespValue CommandDispatcher::dispatch(const resp::RespValue& request) const {
    if (!request.is<resp::Array>() || request.as<resp::Array>().isNull() ||
        request.as<resp::Array>().elements->empty()) {
        return errorReply("ERR Protocol error: expected a non-empty array of bulk strings");
    }

    // Collect arguments, rejecting anything but bulk strings
    std::vector<std::string> args;
    args.reserve(request.as<resp::Array>().elements->size());
    for (const resp::RespValue& element : *request.as<resp::Array>().elements) {
        if (!element.is<resp::BulkString>() || element.as<resp::BulkString>().isNull()) {
            return errorReply("ERR Protocol error: expected a non-empty array of bulk strings");
        }
        args.push_back(*element.as<resp::BulkString>().data);
    }

    // Look up the command and run it
    const std::string name = toLower(args.front());
    auto it = commands_.find(name);
    if (it == commands_.end()) {
        return errorReply("ERR unknown command '" + args.front() + "'");
    }
    const Command& command = *it->second;
    if (!arityMatches(command.arity(), args.size())) {
        return errorReply("ERR wrong number of arguments for '" + name + "' command");
    }
    return command.execute(args, store_);
}

}  // namespace miniredis::command
