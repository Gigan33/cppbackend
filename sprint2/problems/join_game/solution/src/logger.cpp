#include "logger.h"
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes/value_extraction.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <iostream>

namespace expr = boost::log::expressions;

void MyFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    json::object log_obj;

    // 1. Извлекаем TimeStamp, добавленный add_common_attributes()
    auto time_attr = logging::extract<boost::posix_time::ptime>("TimeStamp", rec);
    if (time_attr) {
        log_obj["timestamp"] = boost::posix_time::to_iso_extended_string(*time_attr);
    }

    // 2. Извлекаем data (AdditionalData) из макроса add_value
    if (auto data_attr = logging::extract<json::value>("AdditionalData", rec)) {
        log_obj["data"] = data_attr.get();
    } else {
        log_obj["data"] = json::object{};
    }

    // 3. Извлекаем само текстовое сообщение из макроса логгера
    log_obj["message"] = *rec[expr::smessage];

    // Выводим строго одну строку валидного JSON в поток
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