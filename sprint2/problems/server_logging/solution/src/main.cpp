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
        // 1. Инициализируем логер Практикума
        Logger::GetInstance().Init();

        // Читаем аргументы командной строки
        const std::filesystem::path config_path = argv[1];
        const std::filesystem::path static_dir = argv[2];

        // 2. Загружаем модель игры из JSON
        model::Game game = json_loader::LoadGame(config_path);

        // 3. Создаем io_context
        net::io_context ioc;

        // 4. Добавляем асинхронное отслеживание сигналов остановки (SIGINT, SIGTERM)
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code& ec, int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;
        
        // 5. Создаем strand для последовательного выполнения запросов к API
        auto api_strand = net::make_strand(ioc);
        
        // 6. Создаем RequestHandler (передаем game, static_dir и strand)
        auto handler = std::make_shared<http_handler::RequestHandler>(game, static_dir.string(), api_strand);
        
        // 7. Оборачиваем в декоратор логирования
        http_handler::LoggingHandler<http_handler::RequestHandler> logging_handler(*handler);
        
        // 8. Запускаем HTTP-сервер
        http_server::ServeHttp(ioc, {address, port}, [&logging_handler](auto&& req, auto&& send) {
            logging_handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        // Логируем структурированный старт сервера через твой логер
        json::object start_data;
        start_data["port"] = port;
        start_data["address"] = address.to_string();
        Logger::GetInstance().LogJson("server started", start_data);

        // 10. Запускаем пул потоков для асинхронной работы сервера
        const unsigned num_threads = std::thread::hardware_concurrency();
        RunWorkers(num_threads, [&ioc] {
            ioc.run();
        });

    } catch (const std::exception& ex) {
        json::object data;
        data["code"] = EXIT_FAILURE;
        data["exception"] = ex.what();
        Logger::GetInstance().LogJson("server exited", data);
        std::cerr << "Server exited with exception: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return 0;
}