#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

#include "miniredis/command/command.hpp"
#include "miniredis/command/command_dispatcher.hpp"

namespace miniredis::command {
namespace {

using resp::RespValue;
using Millis = store::KeyValueStore::Millis;

constexpr std::string_view kNotAnInteger = "ERR value is not an integer or out of range";

// --- Argument helpers ---

std::optional<std::int64_t> parseInteger(std::string_view text) {
    std::int64_t value = 0;
    auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc() || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

bool equalsIgnoreCase(std::string_view a, std::string_view b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](unsigned char x, unsigned char y) {
        return std::tolower(x) == std::tolower(y);
    });
}

std::optional<Millis> toMillis(std::int64_t amount, std::int64_t unitMillis) {
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
    if (amount > kMax / unitMillis || amount < kMin / unitMillis) {
        return std::nullopt;
    }
    constexpr std::int64_t kMaxTtlMillis = std::int64_t{100} * 365 * 24 * 60 * 60 * 1000;
    const std::int64_t millis = amount * unitMillis;
    if (millis > kMaxTtlMillis) {
        return std::nullopt;
    }
    return Millis(millis);
}

// --- Commands ---

class PingCommand final : public Command {
public:
    std::string_view name() const override { return "ping"; }
    int arity() const override { return -1; }

    RespValue execute(Args args, store::KeyValueStore&) const override {
        if (args.size() > 2) {
            return errorReply("ERR wrong number of arguments for 'ping' command");
        }
        if (args.size() == 2) {
            return resp::BulkString::of(args[1]);
        }
        return resp::SimpleString("PONG");
    }
};

class GetCommand final : public Command {
public:
    std::string_view name() const override { return "get"; }
    int arity() const override { return 2; }

    RespValue execute(Args args, store::KeyValueStore& store) const override {
        std::optional<std::string> value = store.get(args[1]);
        return value ? resp::BulkString::of(std::move(*value)) : resp::BulkString::null();
    }
};

class SetCommand final : public Command {
public:
    std::string_view name() const override { return "set"; }
    int arity() const override { return -3; }

    RespValue execute(Args args, store::KeyValueStore& store) const override {
        std::optional<Millis> ttl;
        for (std::size_t i = 3; i < args.size(); i += 2) {
            const bool isEx = equalsIgnoreCase(args[i], "EX");
            const bool isPx = equalsIgnoreCase(args[i], "PX");
            if ((!isEx && !isPx) || ttl || i + 1 >= args.size()) {
                return errorReply("ERR syntax error");
            }
            std::optional<std::int64_t> amount = parseInteger(args[i + 1]);
            if (!amount) {
                return errorReply(std::string(kNotAnInteger));
            }
            ttl = *amount > 0 ? toMillis(*amount, isEx ? 1000 : 1) : std::nullopt;
            if (!ttl) {
                return errorReply("ERR invalid expire time in 'set' command");
            }
        }
        store.set(args[1], args[2], ttl);
        return resp::SimpleString("OK");
    }
};

class DelCommand final : public Command {
public:
    std::string_view name() const override { return "del"; }
    int arity() const override { return -2; }

    RespValue execute(Args args, store::KeyValueStore& store) const override {
        std::int64_t deleted = 0;
        for (std::size_t i = 1; i < args.size(); ++i) {
            deleted += store.del(args[i]) ? 1 : 0;
        }
        return resp::Integer{deleted};
    }
};

class ExpireCommand final : public Command {
public:
    std::string_view name() const override { return "expire"; }
    int arity() const override { return 3; }

    RespValue execute(Args args, store::KeyValueStore& store) const override {
        std::optional<std::int64_t> seconds = parseInteger(args[2]);
        if (!seconds) {
            return errorReply(std::string(kNotAnInteger));
        }
        std::optional<Millis> ttl = *seconds <= 0 ? Millis(0) : toMillis(*seconds, 1000);
        if (!ttl) {
            return errorReply("ERR invalid expire time in 'expire' command");
        }
        return resp::Integer{store.expire(args[1], *ttl) ? 1 : 0};
    }
};

class TtlCommand final : public Command {
public:
    std::string_view name() const override { return "ttl"; }
    int arity() const override { return 2; }

    RespValue execute(Args args, store::KeyValueStore& store) const override {
        const std::int64_t millis = store.pttl(args[1]);
        if (millis < 0) {
            return resp::Integer{millis};
        }
        return resp::Integer{(millis + 500) / 1000};
    }
};

}  // namespace

void registerBuiltinCommands(CommandDispatcher& dispatcher) {
    dispatcher.registerCommand(std::make_unique<PingCommand>());
    dispatcher.registerCommand(std::make_unique<GetCommand>());
    dispatcher.registerCommand(std::make_unique<SetCommand>());
    dispatcher.registerCommand(std::make_unique<DelCommand>());
    dispatcher.registerCommand(std::make_unique<ExpireCommand>());
    dispatcher.registerCommand(std::make_unique<TtlCommand>());
}

}  // namespace miniredis::command
