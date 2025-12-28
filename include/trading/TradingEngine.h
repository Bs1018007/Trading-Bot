#pragma once

#include <string>
#include <atomic>
#include <memory>
#include <chrono>
#include <mutex>

#include "core/OrderBookManager.h"
#include "core/SymbolManager.h"
#include "utils/DataLogger.h"
#include "messaging/AeronPublisher.h"
#include "messaging/SBEEncoder.h"       // [NEW] Required for SBE
#include "network/BybitWebSocketClient.h" 

// [HFT STATE MACHINE]
enum class BotState {
    IDLE,
    PLACING_ORDER,
    WORKING,
    IN_POSITION,
    CANCELLING,
    RECOVERING
};

class TradingEngine {
public:
    TradingEngine(
        const std::string& symbol,
        OrderBookManager& obm,
        SymbolManager& sm,
        DataLogger& logger,
        BybitWebSocketClient* trade_client,
        std::shared_ptr<AeronPublisher> aeron_pub
    );

    // Main Loop
    void run_trading_cycle();

    // Async Callback
    void on_order_update(const std::string& order_id, const std::string& status);

private:
    // ========================================================================
    // DEPENDENCIES
    // ========================================================================
    std::string symbol_;
    OrderBookManager& orderbook_manager_;
    SymbolManager& symbol_manager_;
    DataLogger& logger_;
    BybitWebSocketClient* trade_client_;
    std::shared_ptr<AeronPublisher> aeron_publisher_;
    
    // [NEW] SBE Encoder for compact binary messaging
    SBEEncoder sbe_encoder_;

    // ========================================================================
    // STATE MANAGEMENT
    // ========================================================================
    std::atomic<BotState> current_state_{BotState::IDLE};
    std::string active_order_id_;
    
    // [NEW] Track the price of our active order for Chase Logic
    double active_order_price_ = 0.0; 

    std::chrono::steady_clock::time_point state_entry_time_;
    std::chrono::steady_clock::time_point position_entry_time_;
    std::chrono::steady_clock::time_point last_status_log_;

    // ========================================================================
    // MARTINGALE STRATEGY PARAMETERS
    // ========================================================================
    double base_quantity_;
    double current_qty_;
    int martingale_step_;
    int max_martingale_steps_;
    double profit_target_percent_;
    double stop_loss_percent_;
    double cumulative_loss_;
    
    // ========================================================================
    // POSITION STATE
    // ========================================================================
    bool is_short_;
    double entry_price_;
    bool position_filled_;
    bool waiting_for_close_;
    
    // ========================================================================
    // PNL & STATISTICS
    // ========================================================================
    double last_pnl_percent_;
    double last_pnl_dollars_;
    int total_trades_ = 0;
    int winning_trades_ = 0;
    double total_profit_ = 0.0;

    const int64_t ORDER_TIMEOUT_MS = 5000;

    // ========================================================================
    // INTERNAL FUNCTIONS
    // ========================================================================
    
    bool validate_market_data();
    void reconcile_state_on_startup();
    
    // Strategy Logic
    void evaluate_entry_signal();
    void manage_open_position();
    void apply_martingale_recovery();
    void monitor_working_order(); // [NEW] Chase Logic
    
    // Position Helpers
    void close_position_with_profit();
    void close_position_with_loss();
    void close_position_and_reset();
    void close_position();
    
    // Execution
    void place_order(double price, bool is_short);
    void handle_timeout();
    
    // Utils
    std::string generate_id();
    void print_statistics();
    void log_status();
};