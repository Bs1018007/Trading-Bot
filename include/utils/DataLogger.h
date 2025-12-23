#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <vector>
#include <cstdint>

class DataLogger {
public:
    explicit DataLogger(const std::string& filename = "trading_data.log");
    ~DataLogger();
    
    void log_orderbook(
        const std::string& symbol,
        double mid_price,
        const std::vector<std::pair<double, double>>& bids,
        const std::vector<std::pair<double, double>>& asks
    );
    
    void log_symbol_subscription(const std::vector<std::string>& symbols);
    void log_statistics(uint64_t messages, uint64_t aeron_published, size_t active_symbols);
    void log_error(const std::string& error_message);
    std::string get_log_path() const;

private:
    std::ofstream log_file;
    mutable std::mutex log_mutex;
    std::string log_file_path;
    
    std::string get_timestamp() const;
};