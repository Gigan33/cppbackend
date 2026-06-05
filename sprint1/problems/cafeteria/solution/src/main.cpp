#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>
#include <atomic>
#include <condition_variable>

#include "cafeteria.h"

using namespace std::literals;

namespace {

static std::mutex cout_mutex;

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::thread> workers;
    workers.reserve(n - 1);
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void PrintHotDogResult(const Result<HotDog>& result, Clock::duration order_duration) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    using namespace std::chrono;

    auto as_seconds = [](auto d) {
        return std::chrono::duration_cast<std::chrono::duration<double>>(d).count();
    };

    std::cout << as_seconds(order_duration) << "> ";

    if (result.HasValue()) {
        auto& hot_dog = result.GetValue();
        std::cout << "Hot dog #" << hot_dog.GetId()
                  << ": bread bake time: " << as_seconds(hot_dog.GetBread().GetBakingDuration())
                  << "s, sausage bake time: " << as_seconds(hot_dog.GetSausage().GetCookDuration()) << "s"
                  << std::endl;
    } else {
        try {
            result.ThrowIfHoldsError();
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "Unknown error" << std::endl;
        }
    }
}

std::vector<HotDog> PrepareHotDogs(int num_orders, unsigned num_threads) {
    net::io_context io{static_cast<int>(num_threads)};

    Cafeteria cafeteria{io};

    std::mutex mut;
    std::vector<HotDog> hotdogs;

    const auto start_time = Clock::now();

    auto num_waiting_threads = std::min<int>(num_threads, num_orders);
    std::atomic<int> waiting_count{num_waiting_threads};
    std::mutex start_mutex;
    std::condition_variable start_cv;

    for (int i = 0; i < num_orders; ++i) {
        net::dispatch(io, [&cafeteria, &hotdogs, &mut, i, start_time, &waiting_count, &start_mutex, &start_cv, num_waiting_threads] {
            {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "Order #" << i << " is scheduled on thread #"
                          << std::this_thread::get_id() << std::endl;
            }

            if (i < num_waiting_threads) {
                if (--waiting_count == 0) {
                    start_cv.notify_all();
                } else {
                    std::unique_lock<std::mutex> lock(start_mutex);
                    start_cv.wait(lock, [&waiting_count] { return waiting_count == 0; });
                }
            }

            cafeteria.OrderHotDog([&hotdogs, &mut, start_time](Result<HotDog> result) {
                const auto duration = Clock::now() - start_time;
                PrintHotDogResult(result, duration);
                if (result.HasValue()) {
                    std::lock_guard lk{mut};
                    hotdogs.emplace_back(std::move(result).GetValue());
                }
            });
        });
    }

    RunWorkers(num_threads, [&io] {
        io.run();
    });

    return hotdogs;
}

void VerifyHotDogs(const std::vector<HotDog>& hotdogs) {
    std::unordered_set<int> hotdog_ids;
    std::unordered_set<int> sausage_ids;
    std::unordered_set<int> bread_ids;

    for (auto& hotdog : hotdogs) {
        {
            auto [_, hotdog_id_is_unique] = hotdog_ids.insert(hotdog.GetId());
            assert(hotdog_id_is_unique);
        }
        {
            auto [_, sausage_id_is_unique] = sausage_ids.insert(hotdog.GetSausage().GetId());
            assert(sausage_id_is_unique);
        }
        {
            auto [_, bread_id_is_unique] = bread_ids.insert(hotdog.GetBread().GetId());
            assert(bread_id_is_unique);
        }
    }
}

}  // namespace

int main() {
    using namespace std::chrono;

    constexpr unsigned num_threads = 4;
    constexpr int num_orders = 20;

    const auto start_time = Clock::now();
    auto hotdogs = PrepareHotDogs(num_orders, num_threads);
    const auto cook_duration = Clock::now() - start_time;

    std::cout << "Cook duration: " << duration_cast<duration<double>>(cook_duration).count() << 's'
              << std::endl;

    assert(hotdogs.size() == num_orders);
    assert(cook_duration >= 7s && cook_duration <= 7.5s);

    VerifyHotDogs(hotdogs);
}
