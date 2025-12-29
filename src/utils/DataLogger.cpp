#include "utils/DataLogger.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <sys/stat.h> // Required for mkdir

DataLogger::DataLogger(const std::string& filename) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
    log_file_path = "logs/" + ss.str() + "_" + filename;
    
    // Create logs directory (Linux/macOS)
    mkdir("logs", 0777);
    
    log_file.open(log_file_path, std::ios::app);
    if (log_file.is_open()) {
        log_file << "========================================\n";
        log_file << "Bybit Trading Bot - Data Log\n";
        log_file << "Start Time: " << ss.str() << "\n";
        log_file << "========================================\n\n";
        log_file.flush();
        std::cout << "✓ Data logger initialized: " << log_file_path << "\n";
    } else {
        std::cerr << "Failed to open log file: " << log_file_path << "\n";
    }
}

DataLogger::~DataLogger() {
    if (log_file.is_open()) {
        log_file << "\n========================================\n";
        log_file << "Log session ended\n";
        log_file << "========================================\n";
        log_file.flush();
        log_file.close();
    }
}

std::string DataLogger::get_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%H:%M:%S");
    return ss.str();
}

void DataLogger::log_orderbook(
    const std::string& symbol,
    double mid_price,
    const std::vector<std::pair<double, double>>& bids,
    const std::vector<std::pair<double, double>>& asks
) {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (!log_file.is_open()) return;
    
    log_file << "[" << get_timestamp() << "] "
              << symbol << " | Fair Price: $" << std::fixed 
              << std::setprecision(2) << mid_price << "\n";
    
    log_file << "  BIDS: ";
    for (size_t i = 0; i < std::min(size_t(5), bids.size()); ++i) {
        log_file << bids[i].first << "(" << bids[i].second << ") ";
    }
    log_file << "\n";
    
    log_file << "  ASKS: ";
    for (size_t i = 0; i < std::min(size_t(5), asks.size()); ++i) {
        log_file << asks[i].first << "(" << asks[i].second << ") ";
    }
    log_file << "\n";
    log_file.flush();
}

void DataLogger::log_symbol_subscription(const std::vector<std::string>& symbols) {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (!log_file.is_open()) return;
    
    log_file << "\n[SUBSCRIPTION] Total symbols: " << symbols.size() << "\n";
    log_file << "Symbols: ";
    for (size_t i = 0; i < symbols.size(); ++i) {
        log_file << symbols[i];
        if (i < symbols.size() - 1) log_file << ", ";
        if ((i + 1) % 10 == 0) log_file << "\n          ";
    }
    log_file << "\n\n";
    log_file.flush();
}

void DataLogger::log_statistics(uint64_t messages, uint64_t aeron_published, size_t active_symbols) {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (!log_file.is_open()) return;
    
    log_file << "\n[STATS] " << get_timestamp()
              << " | Messages: " << messages
              << " | Aeron Published: " << aeron_published
              << " | Active Symbols: " << active_symbols << "\n";
    log_file.flush();
}

void DataLogger::log_error(const std::string& error_message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (!log_file.is_open()) return;
    
    log_file << "\n[ERROR] " << get_timestamp() 
              << " | " << error_message << "\n";
    log_file.flush();
}

std::string DataLogger::get_log_path() const {
    return log_file_path;
}

// [FIXED] Updated to use correct member variables
void DataLogger::log(const std::string& tag, const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex); // Fixed: file_mutex_ -> log_mutex
    
    if (log_file.is_open()) { // Fixed: log_file_ -> log_file
        log_file << "[" << get_timestamp() << "] " // Fixed: get_current_time -> get_timestamp
                  << "[" << tag << "] "
                  << message << "\n";
        
        log_file.flush(); 
    }
}