#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <unistd.h>
#include <chrono>

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
    std::cout << "\n🛑 Signal (" << signum << ") received. Graceful shutdown initiated...\n";
    g_running = false;
}

int main() {
    // 1. Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "========================================\n";
    std::cout << "   🚀 BYBIT HFT ENGINE STARTING...      \n";
    std::cout << "========================================\n";

    // 2. Initialize core components
    BotConfiguration config;
    DataLogger data_logger;
    OrderBookManager orderbook_manager;
    SymbolManager symbol_manager;

    // 3. Initialize Aeron Publisher
    auto aeron_publisher = std::make_shared<AeronPublisher>(
        config.aeron_channel, 
        config.orderbook_stream_id
    );
    
    bool aeron_enabled = false;
    if (config.enable_aeron) {
        if (aeron_publisher->init()) {
            aeron_enabled = true;
            std::cout << "✅ Aeron IPC enabled\n";
        } else {
            std::cerr << "⚠️  Aeron init failed. Running in standalone mode.\n";
        }
    }

    // 4. Initialize WebSocket Clients
    // Public Channel: Market Data (Orderbook)
    BybitWebSocketClient public_client(
        orderbook_manager, 
        symbol_manager, 
        config, 
        data_logger, 
        BybitWebSocketClient::ChannelType::PUBLIC
    );
    
    // Private Channel: Order Execution & Updates
    BybitWebSocketClient trade_client(
        orderbook_manager, 
        symbol_manager, 
        config, 
        data_logger, 
        BybitWebSocketClient::ChannelType::PRIVATE_TRADE
    );

    // 5. Connect WebSocket clients
    std::cout << "\n🔌 Connecting to Bybit WebSocket...\n";
    public_client.connect();
    trade_client.connect();   

    // 6. Start WebSocket service threads
    std::thread public_thread([&]() { 
        std::cout << "  ✓ Public WS thread started\n";
        public_client.run(); 
    });
    
    std::thread trade_thread([&]() { 
        std::cout << "  ✓ Trade WS thread started\n";
        trade_client.run(); 
    });

    // 7. Wait for WebSocket connections
    std::cout << "⏳ Waiting for WebSocket connections";
    int connection_timeout = 0;
    while ((!public_client.is_connected() || !trade_client.is_connected()) && g_running) {
        if (connection_timeout++ > 100) {  // 10 second timeout
            std::cerr << "\n❌ Connection timeout. Exiting.\n";
            g_running = false;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "." << std::flush;
    }
    std::cout << " ✅ Connected!\n";

    if (!g_running) {
        public_client.stop();
        trade_client.stop();
        if (public_thread.joinable()) public_thread.join();
        if (trade_thread.joinable()) trade_thread.join();
        return 1;
    }

    // 8. Authenticate private channel
    std::cout << "\n🔐 Authenticating...\n";
    trade_client.authenticate();
    std::this_thread::sleep_for(std::chrono::seconds(2));  // Wait for auth response

    // 9. Subscribe to trading symbol
    std::string trading_symbol = config.symbols.empty() ? "BTCUSDT" : config.symbols[0];
    std::cout << "📡 Subscribing to " << trading_symbol << "...\n";
    public_client.subscribe_to_symbol(trading_symbol);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));  // Wait for initial data

    // 10. Initialize Trading Engine
    std::cout << "\n🤖 Initializing Trading Engine...\n";
    TradingEngine engine(
        trading_symbol,
        orderbook_manager,
        symbol_manager,
        data_logger,
        &trade_client,
        aeron_publisher
    );

    // 11. [CRITICAL FIX] Start Aeron service thread
    std::thread aeron_service_thread;
    if (aeron_enabled) {
        aeron_service_thread = std::thread([&]() {
            std::cout << "  ✓ Aeron service thread started\n";
            while (g_running) {
                // Service Aeron context to prevent timeout
                if (aeron_publisher) {
                    aeron_publisher->service_context();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            std::cout << "  ✓ Aeron service thread stopped\n";
        });
    }

    // 12. Main Trading Loop
    std::cout << "\n✅ SYSTEM ACTIVE - Running HFT Loop\n";
    std::cout << "═══════════════════════════════════════\n\n";

    uint64_t loop_count = 0;
    auto last_stats = std::chrono::steady_clock::now();

    while (g_running) {
        // Run trading cycle
        engine.run_trading_cycle();
        
        // Print periodic stats
        loop_count++;
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats).count() >= 30) {
            std::cout << "\n📈 System Stats:\n";
            std::cout << "  Loops: " << loop_count << "\n";
            std::cout << "  WS Messages: " << public_client.get_message_count() << "\n";
            if (aeron_enabled) {
                std::cout << "  Aeron Published: " << aeron_publisher->get_messages_sent() << "\n";
                std::cout << "  Aeron Connected: " << (aeron_publisher->is_connected() ? "YES" : "NO") << "\n";
            }
            std::cout << "\n";
            last_stats = now;
        }
        
        // Small sleep to prevent 100% CPU usage
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }

    // 13. Graceful Shutdown
    std::cout << "\n🔻 Shutting down gracefully...\n";
    
    // Stop WebSocket clients
    public_client.stop();
    trade_client.stop();
    
    // Wait for threads to finish
    std::cout << "  ⏳ Waiting for WebSocket threads...\n";
    if (public_thread.joinable()) public_thread.join();
    if (trade_thread.joinable()) trade_thread.join();
    std::cout << "  ✓ WebSocket threads stopped\n";
    
    // Stop Aeron service thread
    if (aeron_service_thread.joinable()) {
        std::cout << "  ⏳ Waiting for Aeron service thread...\n";
        aeron_service_thread.join();
    }
    
    // Final stats
    std::cout << "\n📊 Final Statistics:\n";
    std::cout << "  Total Loops: " << loop_count << "\n";
    std::cout << "  WS Messages: " << public_client.get_message_count() << "\n";
    if (aeron_enabled) {
        std::cout << "  Aeron Published: " << aeron_publisher->get_messages_sent() << "\n";
    }
    
    std::cout << "\n✅ Clean shutdown complete. Goodbye!\n";
    return 0;
}