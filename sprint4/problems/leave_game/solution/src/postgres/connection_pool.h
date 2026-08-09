#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <pqxx/pqxx>

namespace postgres {

class ConnectionPool {
public:
    using ConnectionPtr = std::shared_ptr<pqxx::connection>;

    class ConnectionWrapper {
    public:
        ConnectionWrapper(ConnectionPtr conn, ConnectionPool& pool)
            : conn_{std::move(conn)}, pool_{&pool} {}

        ConnectionWrapper(const ConnectionWrapper&) = delete;
        ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;

        ConnectionWrapper(ConnectionWrapper&& other) noexcept
            : conn_{std::move(other.conn_)}, pool_{other.pool_} {
            other.pool_ = nullptr;
        }

        ~ConnectionWrapper() {
            if (conn_ && pool_) pool_->Release(std::move(conn_));
        }

        pqxx::connection& operator*() const { return *conn_; }
        pqxx::connection* operator->() const { return conn_.get(); }

    private:
        ConnectionPtr conn_;
        ConnectionPool* pool_;
    };

    ConnectionPool(size_t size, const std::string& db_url) {
        pool_.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            pool_.emplace_back(std::make_shared<pqxx::connection>(db_url));
        }
    }

    ConnectionWrapper Acquire() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return !pool_.empty(); });
        auto conn = std::move(pool_.back());
        pool_.pop_back();
        return ConnectionWrapper{std::move(conn), *this};
    }

private:
    void Release(ConnectionPtr conn) {
        std::lock_guard lock(mutex_);
        pool_.push_back(std::move(conn));
        cv_.notify_one();
    }

    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<ConnectionPtr> pool_;
};

}  // namespace postgres