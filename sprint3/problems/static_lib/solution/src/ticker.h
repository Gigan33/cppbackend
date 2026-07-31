#pragma once

#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <chrono>
#include <functional>

namespace util {

namespace net = boost::asio;
namespace sys = boost::system;

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using IoContext = net::io_context;
    using Duration = std::chrono::milliseconds;
    using Handler = std::function<void(Duration)>;

    Ticker(const std::shared_ptr<net::strand<IoContext::executor_type>>& strand, 
           Duration period, 
           Handler handler);

    void Start();

private:
    void OnTimer(const sys::error_code& ec);

    std::shared_ptr<net::strand<IoContext::executor_type>> strand_;
    net::steady_timer timer_;
    Duration period_;
    Handler handler_;
    std::chrono::steady_clock::time_point last_tick_;
};

} // namespace util