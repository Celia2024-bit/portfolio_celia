#pragma once
#include <thread>
#include <functional>
#include <mutex>
#include <atomic>
#include <vector>
#include <algorithm>
#include <iostream>

namespace util {

/**
 * @brief DataNotifier: A thread-per-task notification dispatcher.
 * Optimized for high-frequency kernel scheduling benchmarks (61850 simulation).
 */
template<typename T>
class DataNotifier {
public:
    explicit DataNotifier(std::function<void(const T&)> callback)
        : callback_(std::move(callback)), stop_(false) {}

    // Ensure all threads are joined upon destruction to prevent core dumps
    ~DataNotifier() {
        shutdown();
    }

    /**
     * @brief Dispatches data to the callback in a new detached-style managed thread.
     * @param data The input sample (captured by value for thread safety).
     */
    void AddData(const T& data) {
        // PERFORMANCE NOTE: cleanupFinishedThreads() is temporarily disabled 
        // to isolate raw Kernel scheduling latency and prevent O(N) mutex contention.
        // cleanupFinishedThreads(); 

        std::lock_guard<std::mutex> lock(threads_mutex_);
        if (stop_) return;

        // CRITICAL FIX: Data is captured by VALUE [data]. 
        // This ensures the thread owns a valid copy of the sample even if the 
        // producer scope terminates, preventing race conditions or dangling references.
        active_threads_.emplace_back([this, data]() {
            try {
                if (this->callback_) {
                    // Executes the business logic (e.g., protection algorithms)
                    this->callback_(data); 
                }
            } catch (const std::exception& e) {
                // Exception boundary to prevent worker thread crashes from escalating
                std::cerr << "Worker thread exception: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Worker thread encountered an unknown error." << std::endl;
            }
        });
    }

    /**
     * @brief Gracefully shuts down the notifier, joining all active worker threads.
     */
    void shutdown() {
        bool expected = false;
        // Atomic check-and-set to ensure idempotency
        if (!stop_.compare_exchange_strong(expected, true)) {
            return;
        }

        std::lock_guard<std::mutex> lock(threads_mutex_);
        for (auto& thread : active_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        active_threads_.clear();
    }

private:
    /**
     * @brief Original cleanup logic. 
     * WARNING: High-frequency calls may cause O(N) overhead within the critical section.
     */
    void cleanupFinishedThreads() {
        std::lock_guard<std::mutex> lock(threads_mutex_);
        active_threads_.erase(
            std::remove_if(active_threads_.begin(), active_threads_.end(),
                [](std::thread& t) {
                    // Logic check: In high-speed Linux scheduling, threads may 
                    // finish faster than the management loop can track them.
                    return !t.joinable(); 
                }),
            active_threads_.end());
    }

    std::function<void(const T&)> callback_;
    std::vector<std::thread> active_threads_;
    std::mutex threads_mutex_;
    std::atomic<bool> stop_;
};

} // namespace util