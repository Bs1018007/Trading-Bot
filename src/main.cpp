#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <unistd.h> 

#include "config/BotConfiguration.h"
#include "core/OrderBookManager.h"
#include "core/SymbolManager.h"
#include "network/BybitWebSocketClient.h"
#include "trading/TradingEngine.h"
#include "utils/DataLogger.h"
#include "messaging/AeronPublisher.h"

// Global shutdown flag
std::atomic<bool> g_running{true};

void signal_handler(int signum) {
    std::cout << "\n🛑 Signal (" << signum << ") received. Stopping...\n";
    g_running = false;
}

int main() {
    // 1. Signals
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "========================================\n";
    std::cout << "   🚀 BYBIT HFT ENGINE STARTING...      \n";
    std::cout << "========================================\n";

    // 2. Config & Core
    BotConfiguration config;
    DataLogger data_logger;
    OrderBookManager orderbook_manager;
    SymbolManager symbol_manager;

    // 3. Init Aeron
    auto aeron_publisher = std::make_shared<AeronPublisher>(
        config.aeron_channel, config.orderbook_stream_id
    );
    if (config.enable_aeron) {
        if (!aeron_publisher->init()) {
            std::cerr << "⚠️  Aeron init failed. Running standalone.\n";
        }
    }

    // 4. Init Clients
    // Public: Market Data
    BybitWebSocketClient public_client(
        orderbook_manager, symbol_manager, config, data_logger, 
        BybitWebSocketClient::ChannelType::PUBLIC
    );
    
    // Private: Execution
    BybitWebSocketClient trade_client(
        orderbook_manager, symbol_manager, config, data_logger, 
        BybitWebSocketClient::ChannelType::PRIVATE_TRADE
    );

    // 5. Connect & Thread
    public_client.connect();
    trade_client.connect();

    std::thread public_thread([&]() { public_client.run(); });
    std::thread trade_thread([&]() { trade_client.run(); });

    // 6. Warmup Wait Loop
    std::cout << "⏳ Waiting for connections...";
    while (!public_client.is_connected() || !trade_client.is_connected()) {
        if (!g_running) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        std::cout << "." << std::flush;
    }
    std::cout << " OK!\n";

    // 7. Auth & Subscribe
    trade_client.authenticate();
    std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait for Auth Ack

    std::string trading_symbol = config.symbols.empty() ? "BTCUSDT" : config.symbols[0];
    public_client.subscribe_to_symbol(trading_symbol);

    // 8. Init Engine
    // [FIX] Passing &trade_client (Pointer) correctly here
    TradingEngine engine(
        trading_symbol,
        orderbook_manager,
        symbol_manager,
        data_logger,
        &trade_client, 
        aeron_publisher
    );

    // 9. HOT LOOP
    std::cout << "✅ SYSTEM ACTIVE. Running HFT Loop.\n";
    while (g_running) {
        engine.run_trading_cycle();
        // Yield CPU slightly to avoid 100% usage on laptop
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }

    // 10. Shutdown
    std::cout << "🔻 Shutting down...\n";
    public_client.stop();
    trade_client.stop();
    
    if (public_thread.joinable()) public_thread.join();
    if (trade_thread.joinable()) trade_thread.join();

    return 0;
}