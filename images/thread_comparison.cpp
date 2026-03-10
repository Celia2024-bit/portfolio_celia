#include "DataNotifier.h"
#include <iostream>
#include <chrono>
#include <atomic>
#include <thread>
#include <vector>

/**
 * @brief 61850 Stress Test Simulation
 * Purpose: To evaluate the kernel scheduling precision and throughput 
 * of Linux vs. Windows under high-frequency sub-cycle (10ms) intervals.
 */
int main() {
    // --- Benchmark Configuration: Strict 61850 Pulse Mode ---
    const int total_seconds = 5;
    const int intervals_per_sec = 100; // Trigger every 10ms (1/100 sec)
    const int calls_per_interval = 16;  // 16 AddData calls per 10ms burst
    
    std::atomic<int> processed_count{0};
    
    // Simulate real-world protection logic computational overhead
    auto callback = [&](const int& data) {
        volatile double d = 0;
        for(int i = 0; i < 300; ++i) d += i * 0.1; 
        processed_count.fetch_add(1, std::memory_order_relaxed);
    };

    util::DataNotifier<int> notifier(callback);

    std::cout << "--- 61850 High-Frequency Pulse Simulation ---" << std::endl;
    std::cout << "Target Load: 1600 calls/sec | 16 calls every 10ms burst" << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int s = 0; s < total_seconds; ++s) {
        auto sec_start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < intervals_per_sec; ++i) {
            for (int j = 0; j < calls_per_interval; ++j) {
                // Simulate data injection (4 streams * 4 calculations)
                notifier.AddData(s * 1000 + i); 
            }
            // Strict enforcement of the 10ms half-cycle interval
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        auto sec_end = std::chrono::high_resolution_clock::now();
        auto sec_ms = std::chrono::duration_cast<std::chrono::milliseconds>(sec_end - sec_start).count();
        std::cout << "Second " << s + 1 << " injection cycle completed in: " << sec_ms << " ms" << std::endl;
    }

    std::cout << "\n[Status] Load injection completed. Synchronizing worker threads..." << std::endl;
    
    // Grace period for the OS scheduler to clear the remaining thread queue
    std::this_thread::sleep_for(std::chrono::seconds(2));
    notifier.shutdown();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    int expected = total_seconds * intervals_per_sec * calls_per_interval;
    
    std::cout << "---------------------------------------------------------" << std::endl;
    std::cout << "FINAL MISSION REPORT:" << std::endl;
    std::cout << "Expected Tasks:  " << expected << std::endl;
    std::cout << "Processed Tasks: " << processed_count.load() << std::endl;
    std::cout << "Total Execution Time: " << total_ms << " ms" << std::endl;
    std::cout << "---------------------------------------------------------" << std::endl;

    return 0;
}