#include <atomic>
#include <csignal>
#include <exception>
#include <iostream>
#include <string_view>
#include <vector>

#include "miniredis/app/server_options.hpp"
#include "miniredis/command/command_dispatcher.hpp"
#include "miniredis/net/tcp_server.hpp"
#include "miniredis/store/clock.hpp"
#include "miniredis/store/expiry_reaper.hpp"
#include "miniredis/store/key_value_store.hpp"

namespace {

std::atomic<miniredis::net::TcpServer*> gServer{nullptr};

extern "C" void onShutdownSignal(int) {
    if (miniredis::net::TcpServer* server = gServer.load()) {
        server->stop();
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    using namespace miniredis;

    // Parse command-line options
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    app::ServerOptions options;
    try {
        options = app::parseOptions(args);
    } catch (const std::invalid_argument& e) {
        std::cerr << e.what() << '\n' << app::usage(argv[0]);
        return 2;
    }
    if (options.showHelp) {
        std::cout << app::usage(argv[0]);
        return 0;
    }

    try {
        // Ready the storage, commands and the server
        store::SteadyClock clock;
        store::KeyValueStore store(options.capacity, clock);
        store::ExpiryReaper reaper(store, options.reaperInterval);

        command::CommandDispatcher dispatcher(store);
        command::registerBuiltinCommands(dispatcher);

        net::TcpServer server(options.server, dispatcher);
        server.listen();

        std::signal(SIGPIPE, SIG_IGN);
        gServer.store(&server);
        std::signal(SIGINT, onShutdownSignal);
        std::signal(SIGTERM, onShutdownSignal);

        // Serve
        std::cout << "miniredis listening on " << options.server.bindAddress << ':' << server.port()
                  << " (" << options.server.workerThreads << " worker threads, capacity "
                  << options.capacity << " keys)" << std::endl;
        server.run();
        gServer.store(nullptr);
        std::cout << "miniredis shut down" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
