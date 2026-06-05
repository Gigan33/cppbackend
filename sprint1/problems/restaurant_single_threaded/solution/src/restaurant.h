#pragma once

#include <boost/asio.hpp>
#include <functional>
#include <memory>

namespace net = boost::asio;
namespace sys = boost::system;

class Hamburger;

using OrderHandler = std::function<void(sys::error_code ec, int id, Hamburger* hamburger)>;

class Restaurant {
public:
    explicit Restaurant(net::io_context& io);

    int MakeHamburger(bool with_onion, OrderHandler handler);

private:
    net::io_context& io_;
    int next_order_id_ = 0;
};
