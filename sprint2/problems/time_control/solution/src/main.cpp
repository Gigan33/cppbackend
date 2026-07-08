#include <boost/program_options.hpp>
#include <optional>
#include <iostream>

struct Args {
    std::optional<uint64_t> tick_period; // Период в мс (опциональный)
    std::string config_file;             // Путь к конфигу
    std::string www_root;                // Путь к статике
    bool randomize_spawn_points = false; // Случайный спавн
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;
    using namespace std::literals;

    po::options_description desc{"Allowed options"s};

    Args args;
    // Описываем параметры строго по заданию
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<uint64_t>()->value_name("milliseconds"s), "set tick period")
        ("config-file,c", po::value<std::string>(&args.config_file)->value_name("file"s), "set config file path")
        ("www-root,w", po::value<std::string>(&args.www_root)->value_name("dir"s), "set static files root")
        ("randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points), "spawn dogs at random positions");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"s)) {
        std::cout << desc << std::endl;
        return std::nullopt;
    }

    // Проверяем обязательные параметры
    if (!vm.count("config-file"s)) {
        throw std::runtime_error("Config file path is not specified"s);
    }
    if (!vm.count("www-root"s)) {
        throw std::runtime_error("Static files root is not specified"s);
    }

    // Записываем tick-period, если он есть
    if (vm.count("tick-period"s)) {
        args.tick_period = vm["tick-period"s].as<uint64_t>();
    }

    return args;
}