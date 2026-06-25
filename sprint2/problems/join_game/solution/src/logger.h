#pragma once
#include <boost/log/trivial.hpp>
#include <boost/log/expressions/keyword.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/json.hpp>

namespace json = boost::json;
namespace logging = boost::log;

// Регистрируем ключевое слово для передачи JSON-данных в макросы логгера
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

class Logger {
public:
    static void Init();
};