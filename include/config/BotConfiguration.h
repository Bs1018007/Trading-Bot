#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib> // Required for std::getenv
#include <iostream>

struct BotConfiguration {
    BotConfiguration() {
        // Load credentials from Environment Variables for security
        if (const char* key = std::getenv("BYBIT_API_KEY")) {
            api_key = key;
        } else {
            std::cerr << "⚠️  Warning: BYBIT_API_KEY not found in environment variables.\n";
        }

        if (const char* secret = std::getenv("BYBIT_API_SECRET")) {
            api_secret = secret;
        } else {
            std::cerr << "⚠️  Warning: BYBIT_API_SECRET not found in environment variables.\n";
        }
    }

    // Trading symbols to monitor
    std::vector<std::string> symbols = {"BTCUSDT", "ETHUSDT", "SOLUSDT"};
    
    // API credentials (loaded from constructor)
    std::string api_key;
    std::string api_secret;
    
    // Trading parameters
    double trade_quantity = 0.001; // Adjusted to valid min size for BTC
    int max_orders_per_second = 10; // Realistic rate limit
    bool enable_trading = false;
    
    // Aeron IPC configuration
    bool enable_aeron = true;
    std::string aeron_channel = "aeron:ipc";
    int32_t orderbook_stream_id = 1001;
    int32_t signal_stream_id = 1002;
    
    // Symbol fetching
    bool fetch_all_symbols = true;
};