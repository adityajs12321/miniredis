#include "miniredis/net/tcp_server.hpp"

#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <gtest/gtest.h>

#include "manual_clock.hpp"
#include "miniredis/command/command_dispatcher.hpp"
#include "miniredis/net/socket.hpp"
#include "miniredis/resp/resp_encoder.hpp"
#include "miniredis/resp/resp_parser.hpp"

namespace miniredis::net {
namespace {

using namespace std::chrono_literals;
using resp::Array;
using resp::BulkString;
using resp::Integer;
using resp::RespValue;
using resp::SimpleString;

// --- Helpers ---

class TestClient {
public:
    explicit TestClient(std::uint16_t port) : socket_(::socket(AF_INET, SOCK_STREAM, 0)) {
        if (!socket_.valid()) {
            throw std::runtime_error("socket failed");
        }
        disableSigpipe(socket_.get());
        timeval timeout{5, 0};
        ::setsockopt(socket_.get(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        if (::connect(socket_.get(), reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0) {
            throw std::runtime_error("connect failed");
        }
    }

    void sendRaw(std::string_view bytes) { ASSERT_TRUE(sendAll(socket_.get(), bytes)); }

    RespValue call(std::initializer_list<std::string_view> args) {
        sendRaw(resp::encode(Array::command(args)));
        return read();
    }

    RespValue read() {
        while (true) {
            std::size_t pos = 0;
            if (std::optional<RespValue> v = parser_.parse(buffer_, pos)) {
                buffer_.erase(0, pos);
                return *v;
            }
            if (!fill()) {
                ADD_FAILURE() << "connection closed or timed out before a full reply";
                return BulkString::null();
            }
        }
    }

    bool closedByServer() { return buffer_.empty() && !fill(); }

private:
    bool fill() {
        char chunk[4096];
        const ssize_t n = ::recv(socket_.get(), chunk, sizeof chunk, 0);
        if (n <= 0) {
            return false;
        }
        buffer_.append(chunk, static_cast<std::size_t>(n));
        return true;
    }

    FileDescriptor socket_;
    resp::RespParser parser_;
    std::string buffer_;
};

class TcpServerTest : public ::testing::Test {
protected:
    void SetUp() override { startServer(8); }

    void TearDown() override { stopServer(); }

    void startServer(std::size_t workers) {
        command::registerBuiltinCommands(dispatcher);
        ServerConfig config;
        config.port = 0;
        config.workerThreads = workers;
        server = std::make_unique<TcpServer>(config, dispatcher);
        server->listen();
        serverThread = std::thread([this] { server->run(); });
    }

    void stopServer() {
        if (server) {
            server->stop();
            serverThread.join();
            server.reset();
        }
    }

    TestClient connect() { return TestClient(server->port()); }

    miniredis::testing::ManualClock clock;
    store::KeyValueStore store{10'000, clock};
    command::CommandDispatcher dispatcher{store};
    std::unique_ptr<TcpServer> server;
    std::thread serverThread;
};

// --- Request handling ---

TEST_F(TcpServerTest, AnswersPing) {
    TestClient client = connect();
    EXPECT_EQ(client.call({"PING"}), SimpleString("PONG"));
}

TEST_F(TcpServerTest, FullCommandSetOverTheWire) {
    TestClient client = connect();
    EXPECT_EQ(client.call({"SET", "k", "v"}), SimpleString("OK"));
    EXPECT_EQ(client.call({"GET", "k"}), BulkString::of("v"));
    EXPECT_EQ(client.call({"EXPIRE", "k", "5"}), Integer{1});
    EXPECT_EQ(client.call({"TTL", "k"}), Integer{5});
    clock.advance(5s);
    EXPECT_EQ(client.call({"GET", "k"}), BulkString::null());
    EXPECT_EQ(client.call({"SET", "a", "1"}), SimpleString("OK"));
    EXPECT_EQ(client.call({"DEL", "a", "missing"}), Integer{1});
}

TEST_F(TcpServerTest, PipelinedRequestsAreAnsweredInOrder) {
    TestClient client = connect();
    std::string batch;
    for (int i = 0; i < 100; ++i) {
        resp::encode(Array::command({"SET", "k" + std::to_string(i), std::to_string(i)}), batch);
        resp::encode(Array::command({"GET", "k" + std::to_string(i)}), batch);
    }
    client.sendRaw(batch);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(client.read(), SimpleString("OK"));
        EXPECT_EQ(client.read(), BulkString::of(std::to_string(i)));
    }
}

TEST_F(TcpServerTest, RequestSplitAcrossManyWrites) {
    TestClient client = connect();
    const std::string request = resp::encode(Array::command({"SET", "key", "a fairly long value"}));
    for (char c : request) {
        client.sendRaw(std::string_view(&c, 1));
        std::this_thread::sleep_for(100us);
    }
    EXPECT_EQ(client.read(), SimpleString("OK"));
    EXPECT_EQ(client.call({"GET", "key"}), BulkString::of("a fairly long value"));
}

TEST_F(TcpServerTest, ConcurrentClientsSeeConsistentData) {
    constexpr int kClients = 8;
    constexpr int kOps = 200;
    std::vector<std::thread> threads;
    std::atomic<int> mismatches{0};
    for (int c = 0; c < kClients; ++c) {
        threads.emplace_back([&, c] {
            TestClient client = connect();
            for (int i = 0; i < kOps; ++i) {
                const std::string key = "c" + std::to_string(c) + ":" + std::to_string(i);
                if (client.call({"SET", key, std::to_string(i)}) != RespValue(SimpleString("OK")) ||
                    client.call({"GET", key}) != RespValue(BulkString::of(std::to_string(i)))) {
                    ++mismatches;
                }
            }
        });
    }
    for (std::thread& t : threads) {
        t.join();
    }
    EXPECT_EQ(mismatches.load(), 0);
    EXPECT_EQ(store.size(), static_cast<std::size_t>(kClients * kOps));
}

TEST_F(TcpServerTest, InlineCommandsWork) {
    TestClient client = connect();
    client.sendRaw("SET k hello\r\nGET k\nPING\r\n");
    EXPECT_EQ(client.read(), SimpleString("OK"));
    EXPECT_EQ(client.read(), BulkString::of("hello"));
    EXPECT_EQ(client.read(), SimpleString("PONG"));
}

// --- Errors ---

TEST_F(TcpServerTest, ProtocolErrorRepliesThenCloses) {
    TestClient client = connect();
    client.sendRaw("*1\r\n?garbage\r\n");
    RespValue reply = client.read();
    ASSERT_TRUE(reply.is<resp::SimpleError>()) << reply;
    EXPECT_EQ(reply.as<resp::SimpleError>().message().rfind("ERR Protocol error", 0), 0u);
    EXPECT_TRUE(client.closedByServer());
}

TEST_F(TcpServerTest, CommandErrorsKeepConnectionOpen) {
    TestClient client = connect();
    EXPECT_TRUE(client.call({"NOPE"}).is<resp::SimpleError>());
    EXPECT_EQ(client.call({"PING"}), SimpleString("PONG"));
}

TEST_F(TcpServerTest, StopDisconnectsIdleClientsPromptly) {
    TestClient a = connect();
    TestClient b = connect();
    EXPECT_EQ(a.call({"PING"}), SimpleString("PONG"));

    const auto start = std::chrono::steady_clock::now();
    stopServer();
    EXPECT_LT(std::chrono::steady_clock::now() - start, 2s);
    EXPECT_TRUE(a.closedByServer());
    EXPECT_TRUE(b.closedByServer());
}

// --- Worker pool ---

class SmallPoolTcpServerTest : public TcpServerTest {
protected:
    void SetUp() override { startServer(1); }
};

TEST_F(SmallPoolTcpServerTest, ManyMoreClientsThanWorkers) {
    std::vector<TestClient> clients;
    for (int i = 0; i < 20; ++i) {
        clients.push_back(connect());
    }
    for (int round = 0; round < 3; ++round) {
        for (auto it = clients.rbegin(); it != clients.rend(); ++it) {
            EXPECT_EQ(it->call({"PING"}), SimpleString("PONG"));
        }
    }
}

TEST_F(SmallPoolTcpServerTest, ConcurrentClientsBeyondPoolSize) {
    constexpr int kClients = 16;
    std::vector<std::thread> threads;
    std::atomic<int> ok{0};
    for (int c = 0; c < kClients; ++c) {
        threads.emplace_back([&, c] {
            TestClient client = connect();
            for (int i = 0; i < 100; ++i) {
                const std::string key = "c" + std::to_string(c) + ":" + std::to_string(i);
                client.call({"SET", key, "v"});
                ok += client.call({"GET", key}) == RespValue(BulkString::of("v")) ? 1 : 0;
            }
        });
    }
    for (std::thread& t : threads) {
        t.join();
    }
    EXPECT_EQ(ok.load(), kClients * 100);
}

// --- Large and late requests ---

TEST_F(TcpServerTest, LargeValueRoundTrips) {
    TestClient client = connect();
    const std::string big(5 * 1024 * 1024, 'x');
    EXPECT_EQ(client.call({"SET", "big", big}), SimpleString("OK"));
    EXPECT_EQ(client.call({"GET", "big"}), BulkString::of(big));
}

TEST_F(TcpServerTest, RequestsSentBeforeClientClosesAreStillExecuted) {
    {
        TestClient client = connect();
        client.sendRaw(resp::encode(Array::command({"SET", "k", "v"})));
    }
    TestClient other = connect();
    bool seen = false;
    for (int i = 0; i < 500 && !seen; ++i) {
        seen = other.call({"GET", "k"}) == RespValue(BulkString::of("v"));
        if (!seen) {
            std::this_thread::sleep_for(1ms);
        }
    }
    EXPECT_TRUE(seen);
}

}  // namespace
}  // namespace miniredis::net
