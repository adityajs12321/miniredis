#include "miniredis/net/socket.hpp"

#include <cerrno>

#include <sys/socket.h>
#include <unistd.h>

namespace miniredis::net {

void FileDescriptor::reset(int fd) noexcept {
    if (fd_ >= 0) {
        ::close(fd_);
    }
    fd_ = fd;
}

void disableSigpipe([[maybe_unused]] int fd) noexcept {
#ifdef SO_NOSIGPIPE
    int on = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof on);
#endif
}

bool sendAll(int fd, std::string_view data) noexcept {
#ifdef MSG_NOSIGNAL
    constexpr int kFlags = MSG_NOSIGNAL;
#else
    constexpr int kFlags = 0;
#endif
    while (!data.empty()) {
        const ssize_t n = ::send(fd, data.data(), data.size(), kFlags);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        data.remove_prefix(static_cast<std::size_t>(n));
    }
    return true;
}

}  // namespace miniredis::net
