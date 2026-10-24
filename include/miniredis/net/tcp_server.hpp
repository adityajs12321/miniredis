#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "miniredis/command/command_dispatcher.hpp"
#include "miniredis/concurrency/thread_pool.hpp"
#include "miniredis/net/client_session.hpp"
#include "miniredis/net/socket.hpp"

namespace miniredis::net {

struct ServerConfig {
    std::string bindAddress = "127.0.0.1";
    std::uint16_t port = 6379;
    std::size_t workerThreads = 8;
    int backlog = 128;
};

class TcpServer {
public:
    TcpServer(ServerConfig config, const command::CommandDispatcher& dispatcher);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    void listen();

    std::uint16_t port() const noexcept { return boundPort_; }

    void run();

    void stop() noexcept;

private:
    struct Connection {
        std::unique_ptr<ClientSession> session;
        bool busy = false;
    };

    void acceptOne();
    void dispatchToWorker(int fd, Connection& connection);
    void applyCompletions();
    void wake() noexcept;
    void drainWakePipe() noexcept;
    void shutdownConnections();

    ServerConfig config_;
    const command::CommandDispatcher& dispatcher_;
    FileDescriptor listener_;
    FileDescriptor wakeRead_;
    FileDescriptor wakeWrite_;
    std::uint16_t boundPort_ = 0;
    std::atomic<bool> stopping_{false};

    std::unordered_map<int, Connection> connections_;

    std::mutex completionsMutex_;
    std::vector<std::pair<int, ClientSession::Status>> completions_;

    concurrency::ThreadPool pool_;
};

}  // namespace miniredis::net
