#pragma once

#include <cstddef>
#include <string>

#include "miniredis/command/command_dispatcher.hpp"
#include "miniredis/net/socket.hpp"
#include "miniredis/resp/resp_parser.hpp"

namespace miniredis::net {

class ClientSession {
public:
    enum class Status { kOpen, kClosed };

    static constexpr std::size_t kMaxPendingBytes = 1024 * 1024 * 1024;

    static constexpr std::size_t kMaxReadPerCall = 1024 * 1024;

    ClientSession(FileDescriptor socket, const command::CommandDispatcher& dispatcher);

    int fd() const noexcept { return socket_.get(); }

    Status onReadable();

private:
    bool readAvailable();

    FileDescriptor socket_;
    const command::CommandDispatcher& dispatcher_;
    resp::RespParser parser_;
    std::string pending_;
};

}  // namespace miniredis::net
