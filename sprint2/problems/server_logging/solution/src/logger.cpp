#include "logger.h"
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/attributes/value_extraction.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <iostream>

namespace expr = boost::log::expressions;

void MyFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    json::object log_obj;

    // 1. Извлекаем timestamp и переводим в extended ISO string
    auto timestamp = rec[logging::trivial::severity]; // Фиктивно, атрибуты времени берутся из глобальных
    auto time_attr = logging::extract<boost::posix_time::ptime>("TimeStamp", rec);
    if (time_attr) {
        log_obj["timestamp"] = boost::posix_time::to_iso_extended_string(*time_attr);
    }

    // 2. Извлекаем data (AdditionalData)
    if (auto data_attr = logging::extract<json::value>("AdditionalData", rec)) {
        log_obj["data"] = data_attr.get();
    } else {
        log_obj["data"] = json::object{};
    }

    // 3. Извлекаем message из самого тела сообщения
    log_obj["message"] = *rec[expr::smessage];

    // Выводим строго в поток логгера одну JSON-строку
    strm << json::serialize(log_obj);
}

void Logger::Init() {
    logging::add_common_attributes();
    logging::add_console_log(
        std::cout,
        logging::keywords::format = &MyFormatter,
        logging::keywords::auto_flush = true
    );
}