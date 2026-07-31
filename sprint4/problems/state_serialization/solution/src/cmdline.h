#pragma once

#include <boost/program_options.hpp>
#include <optional>
#include <string>
#include <iostream>

struct Args {
    std::optional<uint64_t> tick_period;
    std::string config_file;
    std::string www_root;
    bool randomize_spawn_points{false};

    std::optional<std::string> state_file;
    std::optional<uint64_t> save_state_period; // в миллисекундах
};

[[nodiscard]] inline std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;
    
    Args args;
    po::options_description desc{"All options"};
    
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<uint64_t>(), "set tick period in milliseconds")
        ("config-file,c", po::value<std::string>(&args.config_file)->required(), "set config file path")
        ("www-root,w", po::value<std::string>(&args.www_root)->required(), "set static files root path")
        ("randomize-spawn-points", "spawn dogs at random positions")
        // 🟢 Новые флаги:
        ("state-file", po::value<std::string>(), "set state file path")
        ("save-state-period", po::value<uint64_t>(), "set save state period in milliseconds");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return std::nullopt;
        }

        po::notify(vm);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing command line: " << e.what() << std::endl;
        std::cerr << desc << std::endl;
        return std::nullopt;
    }

    if (vm.count("tick-period")) {
        args.tick_period = vm["tick-period"].as<uint64_t>();
    }

    if (vm.count("randomize-spawn-points")) {
        args.randomize_spawn_points = true;
    }

    if (vm.count("state-file")) {
        args.state_file = vm["state-file"].as<std::string>();
    }

    if (vm.count("save-state-period")) {
        args.save_state_period = vm["save-state-period"].as<uint64_t>();
    }

    return args;
}