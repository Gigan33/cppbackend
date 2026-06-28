#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>
#include <string>

using namespace std::literals;

struct Args {
    std::vector<std::string> source;
    std::string destination;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    po::options_description desc{"All options"s};

    Args args;
    desc.add_options()
        ("help,h", "Show help")
        ("src,s", po::value(&args.source)->multitoken()->value_name("files"s), "Source file names")
        ("dst,d", po::value(&args.destination)->value_name("file"s), "Destination file name");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"s)) {
        std::cout << desc;
        return std::nullopt;
    }

    if (!vm.count("src"s)) {
        throw std::runtime_error("Source files have not been specified"s);
    }
    if (!vm.count("dst"s)) {
        throw std::runtime_error("Destination file path is not specified"s);
    }

    return args;
}

int main(int argc, char* argv[]) {
    try {
        if (auto args = ParseCommandLine(argc, argv)) {
            std::ofstream out{args->destination, std::ios_base::binary};
            if (!out) {
                throw std::runtime_error{"Failed to open "s + args->destination + " for writing."s};
            }

            for (const std::string& name : args->source) {
                std::ifstream in{name, std::ios_base::binary};
                if (!in) {
                    throw std::runtime_error{"Failed to open "s + name + " for reading."s};
                }
                if (!(out << in.rdbuf())) {
                    throw std::runtime_error{"Writing error"s};
                }
            }
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}