#pragma once
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/json.hpp>
#include <boost/json/src.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace logging = boost::log;
namespace json = boost::json;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

class Logger {
public:
    static Logger& GetInstance() {
        static Logger instance;
        return instance;
    }

    void Init() {
        logging::core::get()->set_filter(
            logging::trivial::severity >= logging::trivial::info
        );
        logging::add_common_attributes();
        
        auto sink = logging::add_console_log(std::cout);
        sink->set_formatter(
            logging::expressions::stream << logging::expressions::smessage
        );
    }

    void LogJson(const std::string& message, const json::object& data) {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &time_t_now);
#else
        localtime_r(&time_t_now, &tm);
#endif
        
        std::stringstream ts;
        ts << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") 
           << "." << std::setfill('0') << std::setw(3) << ms.count();
        
        json::object obj;
        obj["timestamp"] = ts.str();
        obj["message"] = message;
        obj["data"] = data;
        
        std::string output = json::serialize(obj);
        BOOST_LOG_TRIVIAL(info) << output;
    }
};
