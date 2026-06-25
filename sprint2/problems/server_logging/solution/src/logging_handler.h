#pragma once
#include "logger.h"
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/json.hpp>
#include <chrono>

namespace http_handler {

template <typename Handler>
class LoggingHandler {
public:
    explicit LoggingHandler(Handler& next_handler)
        : next_handler_{next_handler} {}

    template <typename Request, typename Send>
    void operator()(Request&& req, Send&& send) {
        // 1. Логируем получение запроса
        boost::json::value req_data{
            {"ip", "127.0.0.1"}, // В учебных целях или вытащить из endpoint если доступно
            {"URI", std::string(req.target())},
            {"method", std::string(req.method_string())}
        };
        BOOST_LOG_TRIVIAL(info) << boost::log::add_value(additional_data, req_data)
                                << "request received";

        auto start_time = std::chrono::steady_clock::now();

        // 2. Оборачиваем send, чтобы перехватить ответ и замерить время
        auto log_send = [this, start_time, send = std::forward<Send>(send)](auto&& response) mutable {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

            std::string content_type = "null";
            auto it = response.find(boost::beast::http::field::content_type);
            if (it != response.end()) {
                content_type = std::string(it->value());
            }

            // Логируем отправку ответа
            boost::json::value resp_data{
                {"response_time", duration},
                {"code", response.result_int()},
                {"content_type", content_type == "null" ? boost::json::value(nullptr) : boost::json::value(content_type)}
            };
            BOOST_LOG_TRIVIAL(info) << boost::log::add_value(additional_data, resp_data)
                                    << "response sent";

            send(std::forward<decltype(response)>(response));
        };

        next_handler_(std::forward<Request>(req), std::move(log_send));
    }

private:
    Handler& next_handler_;
};

} // namespace http_handler