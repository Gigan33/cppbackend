#include "sdk.h"
#include "json_loader.h"
#include "request_handler.h"
#include "http_server.h"
#include "logger.h"
#include "logging_handler.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <memory>

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
        // 1. Инициализируем логирование
        Logger::GetInstance().Init();
        auto& logger = Logger::GetInstance();
        
        // 2. Загружаем модель игры и статику из аргументов командной строки
        model::Game game = json_loader::LoadGame(argv[1]);
        std::string static_dir = argv[2];

        // 3. Настраиваем многопоточный io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 4. Настраиваем асинхронный перехват сигналов остановки (Ctrl+C и деплой)
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&logger, &ioc](const boost::system::error_code&, int) {
            boost::json::object data;
            data["code"] = 0;
            logger.LogJson("server exited", data);
            ioc.stop();
        });

        // 5. Сетевые настройки
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;
        
        // 6. Создаем strand для последовательного выполнения API-запросов
        auto api_strand = net::make_strand(ioc);
        
        // 7. Создаем RequestHandler в куче через shared_ptr, отдавая ему управление
        auto handler = std::make_shared<http_handler::RequestHandler>(game, static_dir, api_strand);
        
        // 8. Декорируем наш обработчик логированием
        http_handler::LoggingHandler<http_handler::RequestHandler> logging_handler(*handler);
        
        // 9. Запускаем HTTP-сервер
        http_server::ServeHttp(ioc, {address, port}, [&logging_handler](auto&& req, auto&& send) {
            logging_handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        // Лoгируем успешный старт
        boost::json::object start_data;
        start_data["port"] = port;
        start_data["address"] = address.to_string();
        logger.LogJson("server started", start_data);

        // 10. Запускаем пул потоков для параллельной обработки
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
        
    } catch (const std::exception& ex) {
        boost::json::object data;
        data["code"] = EXIT_FAILURE;
        data["exception"] = ex.what();
        Logger::GetInstance().LogJson("server exited", data);
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return 0;
}