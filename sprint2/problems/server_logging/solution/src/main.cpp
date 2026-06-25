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
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;
        
        // 1. Создаем strand для API
        auto api_strand = net::make_strand(ioc);
        
        // 2. Создаем RequestHandler в куче через shared_ptr и передаем strand
        auto handler = std::make_shared<http_handler::RequestHandler>(game, static_dir, api_strand);
        
        // 3. Передаем разыменованный handler (объект, а не указатель) в декоратор логирования
        //    Если твой LoggingHandler принимает ссылку, то оборачиваем лямбду вокруг shared_ptr
        http_handler::LoggingHandler<http_handler::RequestHandler> logging_handler(*handler);
        
        // 4. В ServeHttp передаем лямбду, которая вызывает наш logging_handler
        http_server::ServeHttp(ioc, {address, port}, [&logging_handler](auto&& req, auto&& send) {
            logging_handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        json::object start_data;
    } catch (const std::exception& ex) {
        json::object data;
        data["code"] = EXIT_FAILURE;
        data["exception"] = ex.what();
        Logger::GetInstance().LogJson("server exited", data);
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return 0;
}
