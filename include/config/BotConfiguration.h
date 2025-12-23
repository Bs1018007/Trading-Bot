#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct BotConfiguration {
    // Trading symbols to monitor
    std::vector<std::string> symbols = {"BTCUSDT", "ETHUSDT", "SOLUSDT"};
    
    // API credentials
    std::string api_key = "YOUR_API_KEY";
    std::string api_secret = "YOUR_API_SECRET";
    
    // Trading parameters
    double trade_quantity = 0.0001;
    int max_orders_per_second = 10000;
    bool enable_trading = false;
    
    // Aeron IPC configuration
    bool enable_aeron = true;
    std::string aeron_channel = "aeron:ipc";
    int32_t orderbook_stream_id = 1001;
    int32_t signal_stream_id = 1002;
    
    // Symbol fetching
    bool fetch_all_symbols = true;
};