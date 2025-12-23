#include "config/BotConfiguration.h"
#include "utils/DataLogger.h"
#include "core/OrderBookManager.h"
#include "core/SymbolManager.h"
#include "network/BybitRestClient.h"
#include "network/BybitWebSocketClient.h"
#include "utils/PerformanceMonitor.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    BotConfiguration config;
    config.trade_quantity = 0.001;
    config.max_orders_per_second = 10000;
    config.enable_trading = false;
    config.enable_aeron = true;
    
    std::cout << "\n========================================\n";
    std::cout << "BYBIT MULTI-CRYPTO TRADING BOT\n";
    std::cout << "Aeron + SBE + WebSocket\n";
    std::cout << "========================================\n\n";
    
    std::cout << "🔄 Fetching ALL cryptocurrencies from Bybit...\n";
    auto all_symbols = BybitRestClient::fetch_all_usdt_symbols();
    
    if (all_symbols.empty()) {
        std::cerr << "❌ Failed to fetch symbols, using defaults\n";
        config.symbols = {"BTCUSDT", "ETHUSDT", "SOLUSDT"};
    } else {
        config.symbols = all_symbols;
        std::cout << "✓ Successfully fetched " << all_symbols.size() << " cryptocurrencies!\n";
    }
    
    std::cout << "Tracking " << config.symbols.size() << " symbols\n";
    std::cout << "Trade quantity: " << config.trade_quantity << "\n";
    std::cout << "Max orders/sec: " << config.max_orders_per_second << "\n";
    std::cout << "Trading mode: " << (config.enable_trading ? "LIVE" : "DRY RUN") << "\n";
    std::cout << "Aeron IPC: " << (config.enable_aeron ? "ENABLED" : "DISABLED") << "\n\n";
    
    try {
        DataLogger data_logger("bybit_trading.log");
        OrderBookManager orderbook_manager;
        SymbolManager symbol_manager(config.symbols);
        
        data_logger.log_symbol_subscription(config.symbols);
        
        BybitWebSocketClient ws_client(orderbook_manager, symbol_manager, config, data_logger);
        PerformanceMonitor monitor(ws_client, orderbook_manager, data_logger);
        
        ws_client.connect();
        
        std::thread ws_thread([&]() { ws_client.run(); });
        std::thread monitor_thread([&]() { monitor.run(); });
        
        std::cout << "✓ All systems running. Press Ctrl+C to stop.\n\n";
        
        std::this_thread::sleep_for(std::chrono::hours(24));
        
        std::cout << "\nShutting down...\n";
        ws_client.stop();
        monitor.stop();
        
        ws_thread.join();
        monitor_thread.join();
        
        std::cout << "\n========== FINAL STATISTICS ==========\n";
        std::cout << "WebSocket messages: " << ws_client.get_message_count() << "\n";
        std::cout << "Aeron published: " << ws_client.get_aeron_count() << "\n";
        std::cout << "Total symbols tracked: " << orderbook_manager.size() << "\n";
        std::cout << "======================================\n";
        std::cout << "\n✓ Bot stopped cleanly\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}