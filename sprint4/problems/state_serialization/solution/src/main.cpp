#include "sdk.h"
#include "model_serialization.h"
#include "json_loader.h"
#include "request_handler.h"
#include "ticker.h"
#include "http_server.h"
#include "logger.h"
#include "logging_handler.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/program_options.hpp>

#include <iostream>
#include <thread>
#include <vector>
#include <memory>
#include <chrono>
#include <optional>
#include <filesystem>
#include <fstream>

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;

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

struct Args {
    std::optional<uint64_t> tick_period;
    std::string config_file;
    std::string www_root;
    bool randomize_spawn_points = false;
    std::string state_file;
    std::optional<uint64_t> save_state_period;
};

std::optional<Args> ParseCommandLine(int argc, char* argv[]) {
    namespace po = boost::program_options;

    po::options_description desc{"Allowed options"};

    Args args;
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<uint64_t>()->value_name("milliseconds"), "set tick period")
        ("config-file,c", po::value<std::string>(&args.config_file)->value_name("file"), "set config file path")
        ("www-root,w", po::value<std::string>(&args.www_root)->value_name("dir"), "set static files root")
        ("randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points), "spawn dogs at random positions")
        ("state-file", po::value<std::string>(&args.state_file)->value_name("file"), "set state file path")
        ("state-delay", po::value<uint64_t>()->value_name("milliseconds"), "set state save period")
        ("save-state-period", po::value<uint64_t>()->value_name("milliseconds"), "set state save period");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return std::nullopt;
    }

    if (!vm.count("config-file")) {
        throw std::runtime_error("Config file path is not specified");
    }
    if (!vm.count("www-root")) {
        throw std::runtime_error("Static files root is not specified");
    }

    if (vm.count("tick-period")) {
        args.tick_period = vm["tick-period"].as<uint64_t>();
    }

    if (vm.count("state-delay")) {
        args.save_state_period = vm["state-delay"].as<uint64_t>();
    } else if (vm.count("save-state-period")) {
        args.save_state_period = vm["save-state-period"].as<uint64_t>();
    }

    return args;
}

void SaveState(const std::string& state_file_path, const serialization::SavedState& state) {
    if (state_file_path.empty()) {
        return;
    }
    
    std::filesystem::path target_path{state_file_path};

    if (target_path.has_parent_path()) {
        std::filesystem::create_directories(target_path.parent_path());
    }

    std::filesystem::path temp_path = target_path;
    temp_path += ".tmp";

    {
        std::ofstream ofs(temp_path, std::ios::binary);
        boost::archive::text_oarchive oa(ofs);
        oa << state;
    }

    std::filesystem::rename(temp_path, target_path);
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        Logger::Init();

        auto args = ParseCommandLine(argc, argv);
        if (!args) {
            return EXIT_SUCCESS;
        }

        // 1. Загружаем карту и конфигурацию
        model::Game game = json_loader::LoadGame(args->config_file);
        game.SetRandomizeSpawnPoints(args->randomize_spawn_points); 

        // 2. Инициализируем сетевые сущности ДО создания хэндлера
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        auto api_strand = std::make_shared<net::strand<net::io_context::executor_type>>(net::make_strand(ioc));

        bool auto_tick_enabled = args->tick_period.has_value();

        // 3. Создаем хэндлер ПЕРЕД восстановлением состояния
        auto handler = std::make_shared<http_handler::RequestHandler>(
            game, args->www_root, *api_strand, auto_tick_enabled
        );

        // 4. ВОССТАНАВЛИВАЕМ СОСТОЯНИЕ ЧЕРЕЗ ХЭНДЛЕР
        if (!args->state_file.empty() && std::filesystem::exists(args->state_file)) {
            try {
                std::ifstream ifs(args->state_file, std::ios::binary);
                boost::archive::text_iarchive ia(ifs);
                serialization::SavedState saved_state;
                ia >> saved_state;

                // ВЫЗЫВАЕМ МЕТОД У ХЭНДЛЕРА, А НЕ У ИГРЫ
                handler->RestoreState(saved_state); 
                BOOST_LOG_TRIVIAL(info) << "State successfully restored from " << args->state_file;
            } catch (const std::exception& ex) {
                BOOST_LOG_TRIVIAL(error) << "Failed to restore state: " << ex.what();
            }
        }

        // 5. ПЕРЕДАЕМ НАСТРОЙКИ СОХРАНЕНИЯ В ХЭНДЛЕР
        if (!args->state_file.empty()) {
            std::optional<std::chrono::milliseconds> save_period;
            if (args->save_state_period) {
                save_period = std::chrono::milliseconds{*args->save_state_period};
            }
            handler->SetSaveOptions(args->state_file, save_period, &SaveState);
        }

        // --- ПЕРЕХВАТ СИГНАЛОВ И СОХРАНЕНИЕ ПРИ ВЫХОДЕ ---
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc, api_strand, handler, state_file = args->state_file](const boost::system::error_code& ec, int) {
            if (ec) {
                return;
            }
            
            net::dispatch(*api_strand, [&ioc, handler, state_file]() {
                if (!state_file.empty()) {
                    try {
                        SaveState(state_file, handler->GetSerializedState());
                        BOOST_LOG_TRIVIAL(info) << "State saved on shutdown.";
                    } catch (const std::exception& ex) {
                        BOOST_LOG_TRIVIAL(error) << "Failed to save state on shutdown: " << ex.what();
                    }
                }

                boost::json::value exit_data{{"code", 0}};
                BOOST_LOG_TRIVIAL(info) << boost::log::add_value(additional_data, exit_data)
                                        << "server exited";
                ioc.stop();
            });
        });

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;

        http_handler::LoggingHandler<http_handler::RequestHandler> logging_handler(*handler);

        http_server::ServeHttp(ioc, {address, port}, [&logging_handler](auto&& req, auto&& send) {
            logging_handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        // --- НАСТРОЙКА ИГРОВОГО ТИКЕРА (АВТО-ТИК) ---
        std::shared_ptr<util::Ticker> game_ticker;
        if (args->tick_period) {
            std::chrono::milliseconds period{*args->tick_period};
            
            game_ticker = std::make_shared<util::Ticker>(
                api_strand, 
                period,
                [handler](std::chrono::milliseconds delta) {
                    handler->Tick(delta);
                }
            );
            game_ticker->Start();
        }

        boost::json::value start_data{{"port", port}, {"address", address.to_string()}};
        BOOST_LOG_TRIVIAL(info) << boost::log::add_value(additional_data, start_data)
                                << "server started";

        RunWorkers(num_threads, [&ioc] {
            ioc.run();
        });
        
    } catch (const std::exception& ex) {
        boost::json::value exit_data{{"code", EXIT_FAILURE}, {"exception", ex.what()}};
        BOOST_LOG_TRIVIAL(info) << boost::log::add_value(additional_data, exit_data)
                                << "server exited";
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return 0;
}