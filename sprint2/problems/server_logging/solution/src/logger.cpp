#include "logger.h"

// Реализация логгера
void Logger::Init() {
    logging::core::get()->set_filter(
        logging::trivial::severity >= logging::trivial::info
    );
    logging::add_common_attributes();
    auto sink = logging::add_console_log(std::cout);
    sink->set_formatter(
        expr::stream << expr::format("%1%")
    );
}
