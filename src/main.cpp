#include "config/BotConfiguration.h"
#include "utils/DataLogger.h"
#include "core/OrderBookManager.h"
#include "core/SymbolManager.h"
// #include "network/BybitRestClient.h"
#include "network/BybitWebSocketClient.h"
// #include "utils/PerformanceMonitor.h"
#include "trading/TradingEngine.h"
#include "messaging/AeronPublisher.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::string TRADING_SYMBOL = "BTCUSDT";
    
    BotConfiguration config;
    config.enable_trading = true;
    config.enable_aeron = true;
    
    try {
        DataLogger data_logger("trading.log");
        OrderBookManager orderbook_manager;
        SymbolManager symbol_manager;
        
        BybitWebSocketClient ws_client(orderbook_manager, symbol_manager, config, data_logger);
        ws_client.connect();
        
        std::thread ws_thread([&]() { ws_client.run(); });
        
        // Wait for WebSocket connection to establish
        std::cout << "⏳ Waiting for WebSocket connection...\n";
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        // Subscribe to symbol
        std::cout << "\n📡 Subscribing to " << TRADING_SYMBOL << "...\n";
        ws_client.subscribe_to_symbol(TRADING_SYMBOL);
        
        // Wait for subscription confirmation and first orderbook update
        std::cout << "⏳ Waiting for orderbook data...\n";
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        // Verify subscription status
        if (symbol_manager.is_subscribed(TRADING_SYMBOL)) {
            std::cout << "✅ " << TRADING_SYMBOL << " is subscribed!\n";
            auto orderbook = orderbook_manager.get(TRADING_SYMBOL);
            if (orderbook) {
                std::cout << "   Orderbook exists with " << orderbook->get_update_count() << " updates\n";
            }
        } else {
            std::cerr << "❌ Subscription failed for " << TRADING_SYMBOL << "\n";
        }
        std::cout << "\n";
        
        // ✨ NEW: Create Aeron publisher for order buffer
        auto aeron_publisher = std::make_shared<AeronPublisher>(
            config.aeron_channel,
            config.orderbook_stream_id
        );
        
        if (!aeron_publisher->init()) {
            std::cerr << "⚠ Aeron failed to init, continuing without buffer\n";
            aeron_publisher = nullptr;
        }
        
        // Create trading engine with Aeron publisher
        TradingEngine trading_engine(
            TRADING_SYMBOL,
            orderbook_manager,
            symbol_manager,
            data_logger,
            ws_client.get_wsi(),
            aeron_publisher  // ← Pass the Aeron publisher
        );
        
        std::cout << "✅ Starting trading loop with Aeron buffer\n\n";
        
        while (true) {
            trading_engine.run_trading_cycle();
            
            // Print buffer stats
            if (aeron_publisher) {
                auto all_orders = aeron_publisher->get_all_orders();
                std::cout << "📊 Orders in Aeron buffer: " << all_orders.size() << "\n";
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        
        ws_thread.join();
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}