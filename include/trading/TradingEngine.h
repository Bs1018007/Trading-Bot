#pragma once
#include "core/OrderBookManager.h"
#include "core/SymbolManager.h"
#include "messaging/SBEEncoder.h"
#include "messaging/AeronPublisher.h"
#include "utils/DataLogger.h"
#include <libwebsockets.h>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

struct ActiveOrder {
    std::string order_id;
    std::string symbol;
    std::string side;        // "Buy" or "Sell"
    double entry_price;
    double quantity;
    double profit_target;
    double stop_loss_price;  // New: Track SL
    bool has_profit_order;
    uint64_t timestamp;
};

class TradingEngine {
public:
    TradingEngine(
        const std::string& symbol,
        OrderBookManager& obm,
        SymbolManager& sm,
        DataLogger& logger,
        struct lws* wsi,
        std::shared_ptr<AeronPublisher> aeron_pub
    );
    
    // Main trading loop - executes all steps including recovery logic
    void run_trading_cycle();
    
    // Get current state
    bool has_active_order() const { return has_entry_order_ || has_profit_order_; }
    const ActiveOrder* get_active_order() const;

private:
    std::string symbol_;
    OrderBookManager& orderbook_manager_;
    SymbolManager& symbol_manager_;
    DataLogger& logger_;
    struct lws* wsi_;
    std::shared_ptr<AeronPublisher> aeron_publisher_;
    SBEEncoder sbe_encoder_;
    
    // Order tracking
    ActiveOrder current_order_;
    bool has_entry_order_;
    bool has_profit_order_;
    
    // ---------------------------------------------------------
    // STRATEGY CONFIGURATION & STATE
    // ---------------------------------------------------------
    static constexpr double ENTRY_OFFSET_BPS = 0.0001; // 0.01%
    static constexpr double PROFIT_PERCENT   = 0.0005; // 0.05%
    static constexpr double STOP_LOSS_PERCENT= 0.0020; // 0.20%
    static constexpr double FEE_BUFFER       = 0.0006; // 0.06% (Est. Taker+Maker fees)
    static constexpr double BASE_QUANTITY    = 0.001;  // Starting Size
    static constexpr int    MAX_RECOVERY_STEPS = 5;    // Max Doubling
    
    // Dynamic State
    double current_quantity_;   // Changes based on Martingale
    int recovery_step_;         // How many times have we doubled?
    bool is_short_strategy_;    // True = Short/Sell, False = Long/Buy
    
    // ---------------------------------------------------------
    // STEP IMPLEMENTATIONS
    // ---------------------------------------------------------
    bool step1_check_subscription();
    bool step2_parse_orderbook(double& top_bid, double& top_ask);
    void step3_limit_depth_to_10();
    
    void step4_send_to_sbe(double top_bid, double top_ask);
    
    // Updated to support Shorting
    double step5_calculate_entry_price(double top_bid, double top_ask, bool go_short);
    
    bool step6_check_existing_order();
    void step7_cancel_existing_order();
    
    // Updated to support dynamic quantity
    void step8_place_limit_order(bool go_short, double price, double quantity);
    
    // Updated for SL and TP
    void step9_calculate_risk_levels(double entry_price, bool go_short);
    
    // Updated to return outcome: 0=Waiting, 1=Win, -1=Loss
    int step10_monitor_trade_outcome();
    
    void step11_close_position(double price, bool is_profit);
    
    // New: Handle Doubling/Martingale logic
    void step12_handle_recovery_logic(int outcome);
    
    // Helper functions
    void send_websocket_message(const std::string& message);
    std::string generate_order_id();
    void save_to_aeron_buffer(const ActiveOrder& order);
};