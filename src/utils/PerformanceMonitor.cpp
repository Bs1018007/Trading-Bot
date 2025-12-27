#include "utils/PerformanceMonitor.h"
#include <iostream>
#include <thread>
#include <chrono>

PerformanceMonitor::PerformanceMonitor(
    BybitWebSocketClient& ws,
    OrderBookManager& obm,
    DataLogger& logger
) : ws_client_(ws), orderbook_manager_(obm), data_logger_(logger) {}

void PerformanceMonitor::run() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        uint64_t messages = ws_client_.get_message_count();
        uint64_t aeron_pub = ws_client_.get_aeron_count();
        size_t symbols = orderbook_manager_.size();
        
        std::cout << "\n========== PERFORMANCE STATS ==========\n";
        std::cout << "Messages received: " << messages << "\n";
        std::cout << "Aeron published: " << aeron_pub << "\n";
        std::cout << "Active symbols: " << symbols << "\n";
        std::cout << "======================================\n\n";
        
        data_logger_.log_statistics(messages, aeron_pub, symbols);
    }
}

void PerformanceMonitor::stop() {
    running_ = false;
}

