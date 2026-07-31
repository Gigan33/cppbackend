#include "sdk.h"
#include "json_loader.h"
#include "request_handler.h"
#include "http_server.h"
#include "logger.h"
#include "logging_handler.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <filesystem>

using namespace std::literals;
namespace net = boost::asio;

namespace {

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::thread> workers;
    workers.reserve(n - 1);
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
    for (auto& w : workers) {
        if (w.joinable()) w.join();
    }
}

}  // namespace

int main(int argc, const char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: game_server <game-config-json> <static-dir>"sv << std::endl;
        return EXIT_FAILURE;
    }
    
    try {
        // Инициализируем кастомный Boost.Log форматтер
        Logger::Init();

        const std::filesystem::path config_path = argv[1];
        const std::filesystem::path static_dir = argv[2];

        model::Game game = json_loader::LoadGame(config_path);
        net::io_context ioc;

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code& ec, int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;
        
        auto api_strand = net::make_strand(ioc);
        auto handler = std::make_shared<http_handler::RequestHandler>(game, static_dir.string(), api_strand);
        
        http_handler::LoggingHandler<http_handler::RequestHandler> logging_handler(*handler);
        
        http_server::ServeHttp(ioc, {address, port}, [&logging_handler](auto&& req, auto&& send) {
            logging_handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        // ЛОГИРОВАНИЕ СТАРТА строго по заданию через Boost.Log
        json::value start_data{{"port", port}, {"address", address.to_string()}};
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, start_data)
                                << "server started";

        const unsigned num_threads = std::thread::hardware_concurrency();
        RunWorkers(num_threads, [&ioc] {
            ioc.run();
        });

        // ЛОГИРОВАНИЕ УСПЕШНОГО ВЫХОДА
        json::value exit_data{{"code", 0}};
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, exit_data)
                                << "server exited";

    } catch (const std::exception& ex) {
        // ЛОГИРОВАНИЕ КРИТИЧЕСКОГО ВЫХОДА
        json::value exit_data{{"code", EXIT_FAILURE}, {"exception", ex.what()}};
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, exit_data)
                                << "server exited";
        return EXIT_FAILURE;
    }
    
    return 0;
}