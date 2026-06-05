#include "restaurant.h"
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <iostream>
#include <sstream>
#include <mutex>

using namespace std::literals;
using namespace std::chrono;
namespace net = boost::asio;
namespace sys = boost::system;

static std::mutex cout_mutex;

class Logger {
public:
    explicit Logger(std::string id)
        : id_(std::move(id)) {
    }

    void LogMessage(std::string_view message) const {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << id_ << "> [" << duration<double>(steady_clock::now() - start_time_).count()
                  << "s] " << message << std::endl;
    }

private:
    std::string id_;
    steady_clock::time_point start_time_{steady_clock::now()};
};

class Hamburger {
public:
    [[nodiscard]] bool IsCutletRoasted() const {
        return cutlet_roasted_;
    }
    void SetCutletRoasted() {
        if (IsCutletRoasted()) {
            throw std::logic_error("Cutlet has been roasted already"s);
        }
        cutlet_roasted_ = true;
    }

    [[nodiscard]] bool HasOnion() const {
        return has_onion_;
    }
    void AddOnion() {
        if (IsPacked()) {
            throw std::logic_error("Hamburger has been packed already"s);
        }
        AssureCutletRoasted();
        has_onion_ = true;
    }

    [[nodiscard]] bool IsPacked() const {
        return is_packed_;
    }
    void Pack() {
        AssureCutletRoasted();
        is_packed_ = true;
    }

private:
    void AssureCutletRoasted() const {
        if (!cutlet_roasted_) {
            throw std::logic_error("Cutlet has not been roasted yet"s);
        }
    }

    bool cutlet_roasted_ = false;
    bool has_onion_ = false;
    bool is_packed_ = false;
};

class Order : public std::enable_shared_from_this<Order> {
public:
    Order(net::io_context& io, int id, bool with_onion, OrderHandler handler)
        : io_(io)
        , id_(id)
        , with_onion_(with_onion)
        , handler_(std::move(handler))
        , roast_timer_(io_, 1s)
        , marinade_timer_(io_, 2s)
        , logger_(std::to_string(id)) {
    }

    void Execute() {
        logger_.LogMessage("Order has been started.");
        RoastCutlet();
        if (with_onion_) {
            MarinadeOnion();
        }
    }

private:
    void RoastCutlet() {
        logger_.LogMessage("Start roasting cutlet");
        roast_timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnRoasted(ec);
        });
    }

    void OnRoasted(sys::error_code ec) {
        if (ec) {
            logger_.LogMessage("Roast error: " + ec.message());
        } else {
            logger_.LogMessage("Cutlet has been roasted.");
            hamburger_.SetCutletRoasted();
        }
        CheckReadiness(ec);
    }

    void MarinadeOnion() {
        logger_.LogMessage("Start marinading onion");
        marinade_timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnOnionMarinaded(ec);
        });
    }

    void OnOnionMarinaded(sys::error_code ec) {
        if (ec) {
            logger_.LogMessage("Marinade onion error: " + ec.message());
        } else {
            logger_.LogMessage("Onion has been marinaded.");
            onion_marinaded_ = true;
        }
        CheckReadiness(ec);
    }

    void CheckReadiness(sys::error_code ec) {
        if (delivered_) {
            return;
        }
        if (ec) {
            return Deliver(ec);
        }

        if (CanAddOnion()) {
            logger_.LogMessage("Add onion");
            hamburger_.AddOnion();
        }

        if (IsReadyToPack()) {
            Pack();
        }
    }

    bool CanAddOnion() const {
        return hamburger_.IsCutletRoasted() && onion_marinaded_ && !hamburger_.HasOnion();
    }

    bool IsReadyToPack() const {
        return hamburger_.IsCutletRoasted() && (!with_onion_ || hamburger_.HasOnion());
    }

    void Pack() {
        logger_.LogMessage("Packing");
        auto start = steady_clock::now();
        while (steady_clock::now() - start < 500ms) {
        }
        hamburger_.Pack();
        logger_.LogMessage("Packed");
        Deliver({});
    }

    void Deliver(sys::error_code ec) {
        delivered_ = true;
        handler_(ec, id_, ec ? nullptr : &hamburger_);
    }

    net::io_context& io_;
    int id_;
    bool with_onion_;
    OrderHandler handler_;
    Hamburger hamburger_;
    bool onion_marinaded_ = false;
    bool delivered_ = false;
    net::steady_timer roast_timer_;
    net::steady_timer marinade_timer_;
    Logger logger_;
};

int Restaurant::MakeHamburger(bool with_onion, OrderHandler handler) {
    const int order_id = ++next_order_id_;
    std::make_shared<Order>(io_, order_id, with_onion, std::move(handler))->Execute();
    return order_id;
}
