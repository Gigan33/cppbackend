#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>

#include "hotdog.h"
#include "result.h"
#include "ingredients.h"
#include "gascooker.h"

namespace net = boost::asio;

using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_(io) {
    }

    void OrderHotDog(HotDogHandler handler) {
        static int order_counter = 0;
        auto order_id = std::make_shared<int>(++order_counter);
        auto bread = store_.GetBread();
        auto sausage = store_.GetSausage();
        auto cooker = gas_cooker_;
        
        auto ready_count = std::make_shared<int>(0);
        auto result = std::make_shared<Result<HotDog>>(std::make_exception_ptr(std::runtime_error("Not ready")));
        
        auto check_ready = [handler, ready_count, result, bread, sausage, order_id]() {
            if (++(*ready_count) == 2) {
                try {
                    HotDog hotdog(*order_id, sausage, bread);
                    *result = std::move(hotdog);
                    handler(std::move(*result));
                } catch (const std::exception& e) {
                    *result = std::make_exception_ptr(std::runtime_error(e.what()));
                    handler(std::move(*result));
                }
            }
        };
        
        bread->StartBake(*cooker, [bread, cooker, check_ready, this]() {
            auto timer = std::make_shared<net::steady_timer>(io_);
            timer->expires_after(std::chrono::milliseconds(1250));
            timer->async_wait([bread, timer, check_ready](sys::error_code) {
                bread->StopBaking();
                check_ready();
            });
        });
        
        sausage->StartFry(*cooker, [sausage, cooker, check_ready, this]() {
            auto timer = std::make_shared<net::steady_timer>(io_);
            timer->expires_after(std::chrono::milliseconds(1750));
            timer->async_wait([sausage, timer, check_ready](sys::error_code) {
                sausage->StopFry();
                check_ready();
            });
        });
    }

private:
    net::io_context& io_;
    Store store_;
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
};
