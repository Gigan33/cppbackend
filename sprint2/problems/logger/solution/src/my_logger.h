#pragma once

#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <mutex>
#include <thread>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
    auto GetTime() const {
        if (manual_ts_) {
            return *manual_ts_;
        }
        return std::chrono::system_clock::now();
    }

    std::string GetTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &t_c);
#else
        localtime_r(&t_c, &tm);
#endif
        std::stringstream ss;
        ss << std::put_time(&tm, "%F %T");
        return ss.str();
    }

    std::string GetFileTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &t_c);
#else
        localtime_r(&t_c, &tm);
#endif
        std::stringstream ss;
        ss << std::put_time(&tm, "%Y_%m_%d");
        return ss.str();
    }

    void OpenLogFile() {
        std::string date = GetFileTimeStamp();
        if (current_date_ != date) {
            if (log_file_.is_open()) {
                log_file_.close();
            }
            current_date_ = date;
            std::string filename = "/var/log/sample_log_" + date + ".log";
            log_file_.open(filename, std::ios::app);
        }
    }

    Logger() = default;
    Logger(const Logger&) = delete;

public:
    static Logger& GetInstance() {
        static Logger obj;
        return obj;
    }

    // Шаблонная функция Log — реализация прямо в заголовке
    template<class... Ts>
    void Log(const Ts&... args) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string timestamp = GetTimeStamp();
        OpenLogFile();
        
        if (!log_file_.is_open()) {
            return;
        }
        
        log_file_ << timestamp << ": ";
        (log_file_ << ... << args);
        log_file_ << std::endl;
        log_file_.flush();
    }

    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard<std::mutex> lock(mutex_);
        manual_ts_ = ts;
    }

private:
    std::optional<std::chrono::system_clock::time_point> manual_ts_;
    mutable std::mutex mutex_;
    std::ofstream log_file_;
    std::string current_date_;
};
