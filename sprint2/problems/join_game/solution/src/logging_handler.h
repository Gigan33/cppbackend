#pragma once
#include "logger.h"
#include <chrono>
#include <string>

namespace http_handler {

template<typename Handler>
class LoggingHandler {
public:
    explicit LoggingHandler(Handler& handler) : handler_(handler) {}

    template<typename Request, typename Send>
    void operator()(Request&& req, Send&& send) {
        auto start_time = std::chrono::steady_clock::now();
        
        std::string ip = "127.0.0.1";
        
        // Логируем запрос
        {
            auto& logger = Logger::GetInstance();
            json::object data;
            data["ip"] = ip;
            data["URI"] = std::string(req.target());
            data["method"] = std::string(req.method_string());
            logger.LogJson("request received", data);
        }
        
        // Вызываем оригинальный обработчик
        handler_(std::forward<Request>(req), [this, ip, start_time, &send](auto&& response) {
            // Логируем ответ
            {
                auto& logger = Logger::GetInstance();
                auto end_time = std::chrono::steady_clock::now();
                auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
                
                std::string content_type;
                auto ct = response[http::field::content_type];
                if (!ct.empty()) {
                    content_type = std::string(ct);
                }
                
                json::object data;
                data["ip"] = ip;
                data["response_time"] = response_time;
                data["code"] = response.result_int();
                if (content_type.empty()) {
                    data["content_type"] = nullptr;
                } else {
                    data["content_type"] = content_type;
                }
                logger.LogJson("response sent", data);
            }
            send(std::forward<decltype(response)>(response));
        });
    }

private:
    Handler& handler_;
};

} // namespace http_handler
