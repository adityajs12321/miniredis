#include "miniredis/net/client_session.hpp"

#include <array>
#include <cerrno>
#include <optional>

#include <sys/socket.h>

#include "miniredis/resp/resp_encoder.hpp"

namespace miniredis::net {

ClientSession::ClientSession(FileDescriptor socket, const command::CommandDispatcher& dispatcher)
    : socket_(std::move(socket)), dispatcher_(dispatcher) {}

ClientSession::Status ClientSession::onReadable() {
    const bool peerOpen = readAvailable();

    // Execute every complete request in the buffer
    std::string replies;
    std::size_t pos = 0;
    try {
        while (std::optional<resp::RespValue> request = parser_.parseRequest(pending_, pos)) {
            resp::encode(dispatcher_.dispatch(*request), replies);
        }
    } catch (const resp::RespProtocolError& e) {
        resp::encode(command::errorReply(std::string("ERR Protocol error: ") + e.what()), replies);
        sendAll(fd(), replies);
        return Status::kClosed;
    }
    pending_.erase(0, pos);

    // Flush replies
    if (!replies.empty() && !sendAll(fd(), replies)) {
        return Status::kClosed;
    }
    if (pending_.size() > kMaxPendingBytes) {
        sendAll(fd(), resp::encode(command::errorReply("ERR Protocol error: request too large")));
        return Status::kClosed;
    }
    return peerOpen ? Status::kOpen : Status::kClosed;
}

bool ClientSession::readAvailable() {
    std::array<char, 16 * 1024> chunk{};
    std::size_t total = 0;
    while (total < kMaxReadPerCall) {
        const ssize_t n = ::recv(fd(), chunk.data(), chunk.size(), MSG_DONTWAIT);
        if (n > 0) {
            pending_.append(chunk.data(), static_cast<std::size_t>(n));
            total += static_cast<std::size_t>(n);
            continue;
        }
        if (n == 0) {
            return false;
        }
        if (errno == EINTR) {
            continue;
        }
        return errno == EAGAIN || errno == EWOULDBLOCK;
    }
    return true;
}

}  // namespace miniredis::net
