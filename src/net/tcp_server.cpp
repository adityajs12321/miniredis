#include "miniredis/net/tcp_server.hpp"

#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <system_error>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace miniredis::net {
namespace {

[[noreturn]] void throwErrno(const std::string& what) {
    throw std::system_error(errno, std::generic_category(), what);
}

void setNonBlocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        throwErrno("fcntl");
    }
}

}  // namespace

// --- Lifecycle ---

TcpServer::TcpServer(ServerConfig config, const command::CommandDispatcher& dispatcher)
    : config_(std::move(config)), dispatcher_(dispatcher), pool_(config_.workerThreads) {}

TcpServer::~TcpServer() {
    shutdownConnections();
    pool_.shutdown();
}

void TcpServer::listen() {
    // Used to wake poll() from worker threads
    int pipeFds[2];
    if (::pipe(pipeFds) < 0) {
        throwErrno("pipe");
    }
    wakeRead_.reset(pipeFds[0]);
    wakeWrite_.reset(pipeFds[1]);
    setNonBlocking(wakeRead_.get());
    setNonBlocking(wakeWrite_.get());

    // Create the listening socket
    listener_.reset(::socket(AF_INET, SOCK_STREAM, 0));
    if (!listener_.valid()) {
        throwErrno("socket");
    }
    int on = 1;
    ::setsockopt(listener_.get(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);

    // Bind and start listening
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);
    if (::inet_pton(AF_INET, config_.bindAddress.c_str(), &addr.sin_addr) != 1) {
        throw std::invalid_argument("invalid IPv4 bind address: " + config_.bindAddress);
    }
    if (::bind(listener_.get(), reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0) {
        throwErrno("bind " + config_.bindAddress + ":" + std::to_string(config_.port));
    }
    if (::listen(listener_.get(), config_.backlog) < 0) {
        throwErrno("listen");
    }

    // Record the actual port
    socklen_t len = sizeof addr;
    if (::getsockname(listener_.get(), reinterpret_cast<sockaddr*>(&addr), &len) < 0) {
        throwErrno("getsockname");
    }
    boundPort_ = ntohs(addr.sin_port);
}

void TcpServer::run() {
    if (!listener_.valid()) {
        throw std::logic_error("TcpServer::run() called before listen()");
    }

    std::vector<pollfd> fds;
    while (!stopping_.load()) {
        applyCompletions();

        fds.clear();
        fds.push_back({listener_.get(), POLLIN, 0});
        fds.push_back({wakeRead_.get(), POLLIN, 0});
        for (const auto& [fd, connection] : connections_) {
            if (!connection.busy) {
                fds.push_back({fd, POLLIN, 0});
            }
        }

        if (::poll(fds.data(), static_cast<nfds_t>(fds.size()), -1) < 0) {
            if (errno == EINTR) {
                continue;
            }
            throwErrno("poll");
        }
        // Handle ready descriptors
        if (fds[1].revents != 0) {
            drainWakePipe();
        }
        if (stopping_.load()) {
            break;
        }
        if (fds[0].revents & POLLIN) {
            acceptOne();
        }
        for (std::size_t i = 2; i < fds.size(); ++i) {
            if (fds[i].revents == 0) {
                continue;
            }
            auto it = connections_.find(fds[i].fd);
            if (fds[i].revents & POLLNVAL) {
                connections_.erase(it);
            } else {
                dispatchToWorker(it->first, it->second);
            }
        }
    }

    // Break
    listener_.reset();
    shutdownConnections();
    pool_.shutdown();
    connections_.clear();
}

void TcpServer::stop() noexcept {
    stopping_.store(true);
    wake();
}

// --- Connection handling ---

void TcpServer::acceptOne() {
    const int fd = ::accept(listener_.get(), nullptr, nullptr);
    if (fd < 0) {
        if (errno != EINTR && errno != ECONNABORTED && errno != EAGAIN) {
            std::cerr << "accept: " << std::generic_category().message(errno) << '\n';
        }
        return;
    }
    FileDescriptor socket(fd);
    disableSigpipe(fd);
    int on = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof on);

    connections_.emplace(fd, Connection{std::make_unique<ClientSession>(std::move(socket), dispatcher_)});
}

void TcpServer::dispatchToWorker(int fd, Connection& connection) {
    connection.busy = true;
    ClientSession* session = connection.session.get();
    pool_.submit([this, fd, session] {
        ClientSession::Status status = ClientSession::Status::kClosed;
        try {
            status = session->onReadable();
        } catch (const std::exception& e) {
            std::cerr << "session " << fd << ": " << e.what() << '\n';
        }
        {
            std::lock_guard lock(completionsMutex_);
            completions_.emplace_back(fd, status);
        }
        wake();
    });
}

void TcpServer::applyCompletions() {
    std::vector<std::pair<int, ClientSession::Status>> done;
    {
        std::lock_guard lock(completionsMutex_);
        done.swap(completions_);
    }
    for (const auto& [fd, status] : done) {
        auto it = connections_.find(fd);
        if (status == ClientSession::Status::kClosed) {
            connections_.erase(it);
        } else {
            it->second.busy = false;
        }
    }
}

// --- Wake pipe ---

void TcpServer::wake() noexcept {
    if (wakeWrite_.valid()) {
        const char byte = 1;
        [[maybe_unused]] ssize_t ignored = ::write(wakeWrite_.get(), &byte, 1);
    }
}

void TcpServer::drainWakePipe() noexcept {
    char buf[256];
    while (::read(wakeRead_.get(), buf, sizeof buf) > 0) {
    }
}

void TcpServer::shutdownConnections() {
    for (const auto& [fd, connection] : connections_) {
        ::shutdown(fd, SHUT_RDWR);
    }
}

}  // namespace miniredis::net
