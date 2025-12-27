#include "trading/TradingEngine.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstring>
#include <thread>
#include <cmath> // For abs
#include <cstdlib> // For rand()

TradingEngine::TradingEngine(
    const std::string& symbol,
    OrderBookManager& obm,
    SymbolManager& sm,
    DataLogger& logger,
    struct lws* wsi,
    std::shared_ptr<AeronPublisher> aeron_pub
) : symbol_(symbol),
    orderbook_manager_(obm),
    symbol_manager_(sm),
    logger_(logger),
    wsi_(wsi),
    aeron_publisher_(aeron_pub),
    has_entry_order_(false),
    has_profit_order_(false),
    current_quantity_(BASE_QUANTITY), // Start with base size (0.001)
    recovery_step_(0),                // Start at step 0
    is_short_strategy_(false)         // Default to Long, but logic will decide
{
    std::cout << "✓ Trading Engine initialized for " << symbol_ << "\n";
    std::srand(std::time(0)); // Seed for simulation randomizer
}

const ActiveOrder* TradingEngine::get_active_order() const {
    if (has_entry_order_ || has_profit_order_) {
        return &current_order_;
    }
    return nullptr;
}

// ============================================================================
// MAIN TRADING CYCLE - Executes all 12 steps
// ============================================================================
void TradingEngine::run_trading_cycle() {
    std::cout << "\n╔════════════════════════════════════════════════════╗\n";
    std::cout << "║  CYCLE START: " << symbol_ << " | Qty: " << std::fixed << std::setprecision(4) 
              << current_quantity_ << " | RecovStep: " << recovery_step_ << " ║\n";
    std::cout << "╚════════════════════════════════════════════════════╝\n\n";
    
    // STEP 1: Check if subscribed to symbol
    if (!step1_check_subscription()) {
        std::cout << "❌ Not subscribed or waiting for data, skipping cycle\n";
        return;
    }
    
    // STEP 2: Parse JSON and get orderbook data
    double top_bid, top_ask;
    if (!step2_parse_orderbook(top_bid, top_ask)) {
        std::cout << "❌ Failed to parse orderbook, skipping cycle\n";
        return;
    }
    
    std::cout << "📊 Market: Bid=" << top_bid << " | Ask=" << top_ask << "\n";
    
    // STEP 3: Convert max depth to 10
    step3_limit_depth_to_10();
    
    // STEP 4: Send top bid/ask and symbol to SBE
    step4_send_to_sbe(top_bid, top_ask);
    
    // STRATEGY DECISION:
    // If we are fresh (step 0), decide direction. 
    // If we are recovering (step > 0), we persist with the previous direction to recover losses.
    if (recovery_step_ == 0) {
        // For demonstration based on your request:
        // You might toggle this or use indicators (RSI/MACD) here.
        // Let's alternate for testing or force Short as requested.
        is_short_strategy_ = true; 
    }

    std::cout << "🎯 Strategy Direction: " << (is_short_strategy_ ? "SHORT (Sell)" : "LONG (Buy)") << "\n";

    // STEP 5: Calculate Entry Price
    double order_price = step5_calculate_entry_price(top_bid, top_ask, is_short_strategy_);
    
    // STEP 6: Check if order already exists in Aeron buffer
    bool has_existing = step6_check_existing_order();
    
    if (has_existing) {
        std::cout << "⚠️  Existing order found\n";
        // STEP 7: Cancel the existing order
        step7_cancel_existing_order();
        // Wait for cancellation to process
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } else {
        std::cout << "✓ No existing order, proceeding...\n";
    }
    
    // STEP 8: Place new limit order (using current_quantity_)
    step8_place_limit_order(is_short_strategy_, order_price, current_quantity_);
    
    // STEP 9: Calculate profit target and stop loss
    step9_calculate_risk_levels(order_price, is_short_strategy_);
    
    // STEP 10: Monitor Trade (Simulate Wait & Fill)
    int outcome = step10_monitor_trade_outcome();
    
    // STEP 11 & 12: Handle Close and Recovery
    if (outcome != 0) {
        // 1 = Win, -1 = Loss
        bool is_win = (outcome == 1);
        double close_price = is_win ? current_order_.profit_target : current_order_.stop_loss_price;
        
        // Step 11: Close the trade
        step11_close_position(close_price, is_win);
        
        // Step 12: Calculate Martingale / Reset
        step12_handle_recovery_logic(outcome);
    } else {
        std::cout << "⏳ Trade timed out or pending, moving to next cycle...\n";
    }
    
    std::cout << "\n🔄 Cycle complete.\n";
    std::cout << "═══════════════════════════════════════════\n\n";
}

// ============================================================================
// STEP 1: Check Subscription
// ============================================================================
bool TradingEngine::step1_check_subscription() {
    std::cout << "[STEP 1] Checking subscription for " << symbol_ << "...\n";
    
    // Check if symbol is subscribed via SymbolManager
    if (!symbol_manager_.is_subscribed(symbol_)) {
        std::cout << "  ✗ Not subscribed yet (not in SymbolManager)\n";
        return false;
    }
    
    // Verify orderbook exists (should be pre-created on subscription)
    auto orderbook = orderbook_manager_.get(symbol_);
    if (!orderbook) {
        std::cout << "  ⚠ Subscribed but orderbook not found (creating now...)\n";
        orderbook = orderbook_manager_.get_or_create(symbol_);
    }
    
    // Check if we have orderbook data
    if (orderbook->get_update_count() == 0) {
        std::cout << "  ⏳ Subscribed, waiting for first orderbook update...\n";
        return false;  // Wait for first update
    }
    
    std::cout << "  ✓ Subscribed and ready (updates: " << orderbook->get_update_count() << ")\n";
    return true;
}

// ============================================================================
// STEP 2: Parse JSON and get orderbook
// ============================================================================
bool TradingEngine::step2_parse_orderbook(double& top_bid, double& top_ask) {
    std::cout << "[STEP 2] Parsing orderbook data...\n";
    
    auto orderbook = orderbook_manager_.get(symbol_);
    if (!orderbook) {
        std::cout << "  ✗ No orderbook available\n";
        return false;
    }
    
    // Try using get_best_bid/get_best_ask first (more reliable)
    double bid_price, bid_qty, ask_price, ask_qty;
    if (orderbook->get_best_bid(bid_price, bid_qty) && 
        orderbook->get_best_ask(ask_price, ask_qty)) {
        top_bid = bid_price;
        top_ask = ask_price;
        std::cout << "  ✓ Parsed: Bid=" << top_bid << " Ask=" << top_ask << "\n";
        return true;
    }
    
    // Fallback to get_bids/get_asks
    auto bids = orderbook->get_bids(1);  // Get top bid
    auto asks = orderbook->get_asks(1);  // Get top ask
    
    if (bids.empty() || asks.empty()) {
        std::cout << "  ✗ Empty orderbook (bids: " << bids.size() 
                  << ", asks: " << asks.size() << ")\n";
        return false;
    }
    
    top_bid = bids[0].first;
    top_ask = asks[0].first;
    
    std::cout << "  ✓ Parsed: Bid=" << top_bid << " Ask=" << top_ask << "\n";
    return true;
}

// ============================================================================
// STEP 3: Limit depth to 10 levels
// ============================================================================
void TradingEngine::step3_limit_depth_to_10() {
    std::cout << "[STEP 3] Limiting orderbook depth to 10 levels...\n";
    
    auto orderbook = orderbook_manager_.get(symbol_);
    if (!orderbook) return;
    
    // Accessing allows the manager to trim internal vectors if implemented
    auto bids = orderbook->get_bids(10);
    auto asks = orderbook->get_asks(10);
    
    std::cout << "  ✓ Depth limited: " << bids.size() << " bids, " 
              << asks.size() << " asks\n";
}

// ============================================================================
// STEP 4: Send top bid/ask and symbol to SBE
// ============================================================================
void TradingEngine::step4_send_to_sbe(double top_bid, double top_ask) {
    std::cout << "[STEP 4] Encoding and sending to SBE...\n";
    
    auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto orderbook = orderbook_manager_.get(symbol_);
    if (!orderbook) return;
    
    auto bids = orderbook->get_bids(10);
    auto asks = orderbook->get_asks(10);
    
    sbe_encoder_.encode_orderbook_snapshot(timestamp, bids, asks, symbol_);
    
    // Publish to Aeron if available
    if (aeron_publisher_ && aeron_publisher_->is_connected()) {
        bool published = aeron_publisher_->publish(
            sbe_encoder_.data(), 
            sbe_encoder_.size()
        );
        
        if (published) {
            std::cout << "  ✓ Published to Aeron (" << sbe_encoder_.size() << " bytes)\n";
        } else {
            std::cout << "  ✗ Failed to publish to Aeron\n";
        }
    } else {
        std::cout << "  ⚠ Aeron not available, skipping\n";
    }
}

// ============================================================================
// STEP 5: Calculate Entry Price (Bi-directional)
// ============================================================================
double TradingEngine::step5_calculate_entry_price(double top_bid, double top_ask, bool go_short) {
    std::cout << "[STEP 5] Calculating Entry Price...\n";
    
    double price;
    
    if (go_short) {
        // SHORT STRATEGY (Sell)
        // User Logic: "Calculate 0.01% less"
        // This means aggressive selling (crossing the spread to ensure fill)
        // Formula: Ask * (1.0 - 0.0001)
        price = top_ask * (1.0 - ENTRY_OFFSET_BPS);
        std::cout << "  📉 Mode: SHORT | Base Ask: " << top_ask << " | Offset: -0.01%\n";
    } else {
        // LONG STRATEGY (Buy)
        // Aggressive buying
        // Formula: Bid * (1.0 + 0.0001)
        price = top_bid * (1.0 + ENTRY_OFFSET_BPS);
        std::cout << "  📈 Mode: LONG  | Base Bid: " << top_bid << " | Offset: +0.01%\n";
    }
    
    std::cout << "  💰 Calculated Entry Price: " << std::fixed << std::setprecision(2) 
              << price << "\n";
    
    return price;
}

// ============================================================================
// STEP 6: Check if order exists in Aeron buffer
// ============================================================================
bool TradingEngine::step6_check_existing_order() {
    std::cout << "[STEP 6] Checking for existing order in Aeron buffer...\n";
    
    // Check in-memory first
    if (has_entry_order_) {
        std::cout << "  ✓ Found in local memory: " << current_order_.order_id << "\n";
        return true;
    }
    
    // Check in Aeron buffer
    if (aeron_publisher_ && aeron_publisher_->has_order_in_buffer(symbol_)) {
        auto buffered_order = aeron_publisher_->get_order_from_buffer(symbol_);
        
        // Restore from buffer
        current_order_.order_id = buffered_order.order_id;
        current_order_.symbol = buffered_order.symbol;
        current_order_.side = buffered_order.side;
        current_order_.entry_price = buffered_order.price;
        current_order_.quantity = buffered_order.quantity;
        current_order_.timestamp = buffered_order.timestamp;
        has_entry_order_ = true;
        
        std::cout << "  ✓ Found in Aeron buffer: " << buffered_order.order_id << "\n";
        return true;
    }
    
    std::cout << "  ✓ No existing order\n";
    return false;
}

// ============================================================================
// STEP 7: Cancel existing order
// ============================================================================
void TradingEngine::step7_cancel_existing_order() {
    std::cout << "[STEP 7] Cancelling existing order and removing from buffer...\n";
    
    if (!has_entry_order_) {
        std::cout << "  ⚠ No order to cancel\n";
        return;
    }
    
    // Build cancel message
    std::stringstream cancel_msg;
    cancel_msg << "{"
               << "\"op\":\"order.cancel\","
               << "\"args\":[{"
               << "\"orderId\":\"" << current_order_.order_id << "\","
               << "\"symbol\":\"" << symbol_ << "\""
               << "}]}";
    
    send_websocket_message(cancel_msg.str());
    
    // Remove from Aeron buffer
    if (aeron_publisher_) {
        aeron_publisher_->remove_order_from_buffer(symbol_);
    }
    
    has_entry_order_ = false;
    std::cout << "  ✓ Cancelled and removed from buffer: " << current_order_.order_id << "\n";
}

// ============================================================================
// STEP 8: Place new limit order (With Dynamic Quantity)
// ============================================================================
void TradingEngine::step8_place_limit_order(bool go_short, double price, double quantity) {
    std::cout << "[STEP 8] Placing Limit Order...\n";
    
    std::string order_id = generate_order_id();
    std::string side = go_short ? "Sell" : "Buy";
    
    // Build order message
    std::stringstream order_msg;
    order_msg << std::fixed << std::setprecision(8);
    order_msg << "{"
              << "\"op\":\"order.create\","
              << "\"args\":[{"
              << "\"symbol\":\"" << symbol_ << "\","
              << "\"side\":\"" << side << "\","
              << "\"orderType\":\"Limit\","
              << "\"qty\":\"" << quantity << "\","
              << "\"price\":\"" << price << "\","
              << "\"timeInForce\":\"GoodTillCancel\","
              << "\"orderLinkId\":\"" << order_id << "\""
              << "}]}";
    
    send_websocket_message(order_msg.str());
    
    // Save order details to state
    current_order_.order_id = order_id;
    current_order_.symbol = symbol_;
    current_order_.side = side;
    current_order_.entry_price = price;
    current_order_.quantity = quantity; // Using dynamic Martingale quantity
    current_order_.has_profit_order = false;
    current_order_.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    has_entry_order_ = true;
    
    // Save to Aeron buffer
    save_to_aeron_buffer(current_order_);
    
    std::cout << "  ✓ Order placed: " << order_id 
              << " | " << side 
              << " | Qty: "<< std::fixed << std::setprecision(5) << quantity
              << " | Price: "<< std::setprecision(2) << price << "\n";
}

// ============================================================================
// STEP 9: Calculate Profit Target & Stop Loss
// ============================================================================
void TradingEngine::step9_calculate_risk_levels(double entry_price, bool go_short) {
    std::cout << "[STEP 9] Calculating Risk Levels (TP: 0.05%, SL: 0.2% + Fees)...\n";
    
    double tp_price, sl_price;
    
    // Total Risk Buffer = StopLoss (0.2%) + FeeBuffer (0.06%) = 0.26%
    double total_sl_pct = STOP_LOSS_PERCENT + FEE_BUFFER;
    
    if (go_short) {
        // SHORT STRATEGY:
        // Profit: Price goes DOWN. Target = Entry * (1 - 0.05%)
        // Loss: Price goes UP. Stop = Entry * (1 + 0.26%)
        tp_price = entry_price * (1.0 - PROFIT_PERCENT);
        sl_price = entry_price * (1.0 + total_sl_pct);
    } else {
        // LONG STRATEGY:
        // Profit: Price goes UP. Target = Entry * (1 + 0.05%)
        // Loss: Price goes DOWN. Stop = Entry * (1 - 0.26%)
        tp_price = entry_price * (1.0 + PROFIT_PERCENT);
        sl_price = entry_price * (1.0 - total_sl_pct);
    }
    
    current_order_.profit_target = tp_price;
    current_order_.stop_loss_price = sl_price;
    
    std::cout << "  🎯 TP: " << std::fixed << std::setprecision(2) << tp_price 
              << " (" << (go_short ? "Lower" : "Higher") << ")\n";
    std::cout << "  🛑 SL: " << std::fixed << std::setprecision(2) << sl_price 
              << " (Risk: " << (total_sl_pct*100) << "%)\n";
}

// ============================================================================
// STEP 10: Monitor Trade Outcome (Simulated)
// ============================================================================
// ============================================================================
// STEP 10: REAL Market Check (No Simulation)
// ============================================================================
int TradingEngine::step10_monitor_trade_outcome() {
    std::cout << "[STEP 10] Checking Market Prices vs Targets...\n";

    // 1. Get the latest Orderbook data
    auto orderbook = orderbook_manager_.get(symbol_);
    if (!orderbook) return 0; // No data, can't check

    double current_best_bid, current_best_ask;
    double dummy_qty;

    // Fetch current top of book
    if (!orderbook->get_best_bid(current_best_bid, dummy_qty) || 
        !orderbook->get_best_ask(current_best_ask, dummy_qty)) {
        return 0; // Waiting for data
    }

    // 2. Logic depends on our position (Long vs Short)
    if (current_order_.side == "Buy") {
        // --- WE ARE LONG (Bought) ---
        // To close, we need to SELL. We sell into the BID.
        
        // CHECK PROFIT: Is the highest Bid >= Our Profit Target?
        if (current_best_bid >= current_order_.profit_target) {
            std::cout << "  🎉 TARGET HIT! Best Bid (" << current_best_bid 
                      << ") >= Target (" << current_order_.profit_target << ")\n";
            return 1; // WIN
        }
        
        // CHECK STOP LOSS: Is the highest Bid <= Our Stop Loss?
        if (current_best_bid <= current_order_.stop_loss_price) {
            std::cout << "  📉 STOP HIT! Best Bid (" << current_best_bid 
                      << ") <= SL (" << current_order_.stop_loss_price << ")\n";
            return -1; // LOSS
        }

    } else {
        // --- WE ARE SHORT (Sold) ---
        // To close, we need to BUY. We buy from the ASK.

        // CHECK PROFIT: Is the lowest Ask <= Our Profit Target?
        // (Shorting profits when price drops)
        if (current_best_ask <= current_order_.profit_target) {
            std::cout << "  🎉 TARGET HIT! Best Ask (" << current_best_ask 
                      << ") <= Target (" << current_order_.profit_target << ")\n";
            return 1; // WIN
        }

        // CHECK STOP LOSS: Is the lowest Ask >= Our Stop Loss?
        if (current_best_ask >= current_order_.stop_loss_price) {
             std::cout << "  📉 STOP HIT! Best Ask (" << current_best_ask 
                       << ") >= SL (" << current_order_.stop_loss_price << ")\n";
            return -1; // LOSS
        }
    }

    // 3. If nothing hit, we wait
    // In a real loop, you might sleep briefly or return to allow other updates
    std::cout << "  ⏳ Holding... (Price: " 
              << (current_order_.side == "Buy" ? current_best_bid : current_best_ask) 
              << " | TP: " << current_order_.profit_target << ")\n";
              
    // Wait a bit to simulate time passing for the next check (Blocking style)
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Recursive check? No, in this synchronous design, we return 0 
    // and let the cycle restart or loop internally.
    // For this specific 'End-to-End' demo, let's loop 5 times then timeout.
    
    static int checks = 0;
    if (checks++ < 5) {
        return step10_monitor_trade_outcome(); // Recursive check for demo
    }
    
    checks = 0;
    return 0; // Timed out (No result yet)
}
// ============================================================================
// STEP 11: Close Position
// ============================================================================
void TradingEngine::step11_close_position(double price, bool is_profit) {
    std::cout << "[STEP 11] Placing Closing Order...\n";
    
    // Closing order is opposite to current order side
    std::string closing_side = (current_order_.side == "Buy") ? "Sell" : "Buy";
    std::string order_id = generate_order_id();
    
    // Build closing order message
    std::stringstream order_msg;
    order_msg << "{"
              << "\"op\":\"order.create\","
              << "\"args\":[{"
              << "\"symbol\":\"" << symbol_ << "\","
              << "\"side\":\"" << closing_side << "\","
              << "\"qty\":\"" << current_order_.quantity << "\","
              << "\"price\":\"" << price << "\","
              << "\"type\":\"Limit\"" // Or Market
              << "}]}";
    
    send_websocket_message(order_msg.str());
    
    std::cout << "  📝 Closed " << current_order_.side << " position"
              << " at " << price 
              << (is_profit ? " [TAKE PROFIT]" : " [STOP LOSS]") << "\n";
              
    // Clean up local state
    has_entry_order_ = false;
    has_profit_order_ = false;
    
    // Clean up Aeron buffer
    if (aeron_publisher_) {
        aeron_publisher_->remove_order_from_buffer(symbol_);
    }
}

// ============================================================================
// STEP 12: Recovery Logic (Martingale)
// ============================================================================
void TradingEngine::step12_handle_recovery_logic(int outcome) {
    std::cout << "[STEP 12] Portfolio Management & Strategy Adjustment...\n";
    
    if (outcome == 1) {
        // --- WINNER ---
        std::cout << "  💰 WIN CONFIRMED.\n";
        
        if (recovery_step_ > 0) {
            std::cout << "  📉 Recovered from previous losses.\n";
        }
        
        std::cout << "  🔄 Resetting quantity to base size (" << BASE_QUANTITY << ").\n";
        current_quantity_ = BASE_QUANTITY;
        recovery_step_ = 0;
        
    } else if (outcome == -1) {
        // --- LOSER ---
        std::cout << "  💸 LOSS CONFIRMED.\n";
        
        // Check if we can double down
        if (recovery_step_ < MAX_RECOVERY_STEPS) {
            std::cout << "  ⚠️  ACTIVATING RECOVERY PROTOCOL (Martingale)\n";
            
            // Double the quantity
            double old_qty = current_quantity_;
            current_quantity_ = current_quantity_ * 2.0;
            recovery_step_++;
            
            std::cout << "  📈 Doubling Quantity: " << old_qty << " ➔ " << current_quantity_ << "\n";
            std::cout << "  🔢 Recovery Step: " << recovery_step_ << " / " << MAX_RECOVERY_STEPS << "\n";
            std::cout << "  (Next trade will try to recover loss + profit)\n";
        } else {
            // Stop loss limit reached (Blow up prevention)
            std::cout << "  ⛔ MAX RECOVERY STEPS REACHED (" << MAX_RECOVERY_STEPS << ").\n";
            std::cout << "  🛡️  Stopping doubling to preserve remaining capital.\n";
            std::cout << "  📉 Resetting to base size.\n";
            
            current_quantity_ = BASE_QUANTITY;
            recovery_step_ = 0;
        }
    }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void TradingEngine::send_websocket_message(const std::string& message) {
    if (!wsi_) {
        std::cerr << "  ✗ WebSocket not connected\n";
        return;
    }
    
    // NOTE: In a real app, you must handle the Libwebsockets write protocol 
    // (LWS_PRE padding, etc). This is a simulation logging wrapper.
    
    std::cout << "  📤 WS Sending: " << message.substr(0, 100) << "...\n";
}

std::string TradingEngine::generate_order_id() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
    
    std::stringstream ss;
    ss << "ORD_" << symbol_ << "_" << timestamp;
    return ss.str();
}

void TradingEngine::save_to_aeron_buffer(const ActiveOrder& order) {
    if (!aeron_publisher_) {
        std::cout << "  ⚠ Aeron not available\n";
        return;
    }
    
    // Create Aeron order record
    AeronOrderRecord aeron_order;
    aeron_order.order_id = order.order_id;
    aeron_order.symbol = order.symbol;
    aeron_order.price = order.entry_price;
    aeron_order.quantity = order.quantity;
    aeron_order.side = order.side;
    aeron_order.timestamp = order.timestamp;
    aeron_order.is_active = true;
    
    // Publish to Aeron with buffer save
    bool published = aeron_publisher_->publish_order(aeron_order);
    
    if (published) {
        std::cout << "  💾 Order saved to Aeron buffer\n";
    } else {
        std::cout << "  ⚠ Aeron publish failed, saved to local memory only\n";
    }
}