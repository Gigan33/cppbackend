#include "ticker.h"

namespace util {

Ticker::Ticker(const std::shared_ptr<net::strand<IoContext::executor_type>>& strand, 
               Duration period, 
               Handler handler)
    : strand_(strand)
    , timer_(*strand)
    , period_(period)
    , handler_(handler) {}

void Ticker::Start() {
    net::dispatch(*strand_, [self = shared_from_this()] {
        self->last_tick_ = std::chrono::steady_clock::now();
        self->timer_.expires_after(self->period_);
        self->timer_.async_wait([self](const sys::error_code& ec) {
            self->OnTimer(ec);
        });
    });
}

void Ticker::OnTimer(const sys::error_code& ec) {
    if (ec) {
        return; // Таймер был отменен или произошла ошибка
    }
    
    auto current_tick = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<Duration>(current_tick - last_tick_);
    last_tick_ = current_tick;

    try {
        handler_(duration);
    } catch (...) {
        // Защищаем внутренний цикл таймера от вылета исключений из обработчика
    }

    timer_.expires_after(period_);
    timer_.async_wait([self = shared_from_this()](const sys::error_code& ec) {
        self->OnTimer(ec);
    });
}

} // namespace util