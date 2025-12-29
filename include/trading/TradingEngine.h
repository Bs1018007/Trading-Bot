#ifndef TRADING_ENGINE_H
#define TRADING_ENGINE_H

#include <string>
#include <atomic>
#include <chrono>
#include <memory>
#include "core/OrderBookManager.h"
#include "core/SymbolManager.h"
#include "network/BybitWebSocketClient.h"
#include "utils/DataLogger.h"
#include "messaging/AeronPublisher.h"
#include "messaging/SBEEncoder.h"

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

    void run_trading_cycle();
    void on_order_update(const std::string& order_id, const std::string& status);

private:
    // Core components
    std::string symbol_;
    OrderBookManager& orderbook_manager_;
    SymbolManager& symbol_manager_;
    DataLogger& logger_;
    BybitWebSocketClient* trade_client_;
    std::shared_ptr<AeronPublisher> aeron_publisher_;
    SBEEncoder sbe_encoder_;

    // State management
    std::atomic<BotState> current_state_{BotState::IDLE};
    std::chrono::steady_clock::time_point state_entry_time_;
    std::chrono::steady_clock::time_point position_entry_time_;
    std::chrono::steady_clock::time_point last_status_log_;

    // Order tracking
    std::string active_order_id_;
    double active_order_price_ = 0.0;
    double entry_price_ = 0.0;
    bool is_short_ = false;
    bool position_filled_ = false;
    bool waiting_for_close_ = false;

    // Risk parameters
    double base_quantity_;
    double current_qty_;
    int martingale_step_;
    int max_martingale_steps_;
    double profit_target_percent_;
    double stop_loss_percent_;
    double cumulative_loss_;

    // Statistics
    int total_trades_ = 0;
    int winning_trades_ = 0;
    double total_profit_ = 0.0;
    double last_pnl_percent_ = 0.0;
    double last_pnl_dollars_ = 0.0;

    // [NEW] Orderbook staleness detection
    uint64_t last_orderbook_update_ = 0;

    // Constants
    static constexpr int ORDER_TIMEOUT_MS = 5000;

    // Private methods
    bool validate_market_data();
    void evaluate_entry_signal();
    void monitor_working_order();
    void manage_open_position();
    void apply_martingale_recovery();
    
    void close_position();
    void close_position_with_profit();
    void close_position_with_loss();
    void close_position_and_reset();
    
    void place_order(double price, bool is_short);
    void handle_timeout();
    void reconcile_state_on_startup();
    
    std::string generate_id();
    void print_statistics();
    void log_status();
};

#endif // TRADING_ENGINE_H