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
#include <chrono>

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

void PrintHotDogResult(const Result<HotDog>& result) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    if (result.HasValue()) {
        auto& hot_dog = result.GetValue();
        std::cout << "Hot dog #" << hot_dog.GetId() << " is ready" << std::endl;
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

    auto num_waiting_threads = std::min<int>(num_threads, num_orders);
    std::atomic<int> waiting_count{num_waiting_threads};
    std::mutex start_mutex;
    std::condition_variable start_cv;

    for (int i = 0; i < num_orders; ++i) {
        net::dispatch(io, [&cafeteria, &hotdogs, &mut, i, &waiting_count, &start_mutex, &start_cv, num_waiting_threads] {
            {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "Order #" << i << " is scheduled" << std::endl;
            }

            if (i < num_waiting_threads) {
                if (--waiting_count == 0) {
                    start_cv.notify_all();
                } else {
                    std::unique_lock<std::mutex> lock(start_mutex);
                    start_cv.wait(lock, [&waiting_count] { return waiting_count == 0; });
                }
            }

            cafeteria.OrderHotDog([&hotdogs, &mut](Result<HotDog> result) {
                PrintHotDogResult(result);
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
        auto [_, hotdog_id_is_unique] = hotdog_ids.insert(hotdog.GetId());
        assert(hotdog_id_is_unique);
        auto [__, sausage_id_is_unique] = sausage_ids.insert(hotdog.GetSausage().GetId());
        assert(sausage_id_is_unique);
        auto [___, bread_id_is_unique] = bread_ids.insert(hotdog.GetBread().GetId());
        assert(bread_id_is_unique);
    }
}

}  // namespace

int main() {
    constexpr unsigned num_threads = 4;
    constexpr int num_orders = 20;

    auto hotdogs = PrepareHotDogs(num_orders, num_threads);

    std::cout << "Cooked " << hotdogs.size() << " hot dogs" << std::endl;
    assert(hotdogs.size() == num_orders);

    VerifyHotDogs(hotdogs);
    
    std::cout << "All tests passed!" << std::endl;
}
