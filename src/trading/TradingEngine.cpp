/**
 * @file TradingEngine.cpp
 * @brief Core logic for the Martingale Chaser trading bot.
 * * This file implements the state machine, order execution, PnL monitoring,
 * and risk management (Martingale recovery) for the bot.
 * * STRATEGY OVERVIEW:
 * 1. Wait for Market Data (Order Book).
 * 2. Calculate Entry Price (Aggressive Limit/Market Taker).
 * 3. Place Order & Wait for Fill.
 * 4. Once Filled, Monitor PnL (Profit and Loss).
 * 5. If Target Hit -> Close for Profit -> Reset.
 * 6. If Stop Loss Hit -> Close for Loss -> Double Size (Martingale) -> Reverse Direction.
 */

#include "trading/TradingEngine.h"
#include <iostream>
#include <cmath>
#include <thread>
#include <cstring>
#include <iomanip>

// ============================================================================
// CONSTRUCTOR: INITIALIZATION
// ============================================================================
TradingEngine::TradingEngine(
    const std::string& symbol,
    OrderBookManager& obm,
    SymbolManager& sm,
    DataLogger& logger,
    BybitWebSocketClient* trade_client,
    std::shared_ptr<AeronPublisher> aeron_pub
) : symbol_(symbol),
    orderbook_manager_(obm),
    symbol_manager_(sm),
    logger_(logger),
    trade_client_(trade_client),
    aeron_publisher_(aeron_pub)
{
    // Print Strategy Banner
    std::cout << "\n╔════════════════════════════════════════════════════╗\n";
    std::cout << "║   🎲 MARTINGALE CHASER v3.1 (SBE Enabled) 🎲      ║\n";
    std::cout << "╚════════════════════════════════════════════════════╝\n";
    std::cout << "Strategy:         Best Bid/Ask + Chase Logic\n";
    std::cout << "Encoding:         SBE (Simple Binary Encoding)\n";
    std::cout << "Profit Target:    0.05% (0.0005)\n";
    std::cout << "Stop Loss:        -0.10% (-0.001)\n\n";

    // --- 1. Initialize Risk Parameters ---
    base_quantity_ = 0.02;          // Starting trade size (e.g., 0.001 BTC)
    current_qty_ = base_quantity_;   // Current trade size (will double on loss)
    martingale_step_ = 0;            // Tracks consecutive losses
    max_martingale_steps_ = 6;       // Stop doubling after 6 losses to prevent blow-up
    profit_target_percent_ = 0.0005; // 0.05% gain target
    stop_loss_percent_ = -0.001;     // -0.10% loss limit
    cumulative_loss_ = 0.0;          // Total dollars lost in current sequence
    
    // --- 2. Initialize State Variables ---
    is_short_ = false;               // Direction flag (false=Long/Buy, true=Short/Sell)
    position_filled_ = false;        // Is our entry order fully filled?
    waiting_for_close_ = false;      // Are we currently trying to exit?
    last_orderbook_update_ = 0;      // Tracks staleness of market data

    // --- 3. Initial Market Data Wait Loop ---
    // The bot cannot trade without knowing the price. We wait up to 10 seconds.
    std::cout << "⏳ Waiting for initial market data...\n";
    int wait_count = 0;
    while (wait_count < 100) {  // 100 * 100ms = 10 seconds
        auto ob = orderbook_manager_.get(symbol_);
        
        // Check if OrderBook exists and has received at least one update
        if (ob && ob->get_update_count() > 0) {
            double bid, ask, dummy;
            // Check if we can read valid best Bid/Ask prices
            if (ob->get_best_bid(bid, dummy) && ob->get_best_ask(ask, dummy)) {
                if (bid < ask) {  // Ensure spread is valid (Bid must be lower than Ask)
                    std::cout << "✅ Market data ready: Bid=" << bid 
                              << " Ask=" << ask 
                              << " Spread=$" << (ask - bid) << "\n";
                    last_orderbook_update_ = ob->get_update_count();
                    break; // Exit loop, we are ready!
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Sleep to save CPU
        wait_count++;
    }
    
    if (wait_count >= 100) {
        std::cerr << "⚠️ WARNING: Started without valid market data. Bot may pause.\n";
    }

    // --- 4. State Recovery ---
    // If the bot crashed, check Aeron buffer for active orders to resume management.
    reconcile_state_on_startup();

    // --- 5. Register Callback ---
    // Tell the WebSocket client to call OUR function (on_order_update) when an order changes status.
    if (trade_client_) {
        trade_client_->set_order_update_callback(
            [this](const std::string& id, const std::string& status, const std::string& sym) {
                // Only process updates for OUR active order ID or symbol
                if (sym == symbol_ || (sym.empty() && id == active_order_id_)) {
                    this->on_order_update(id, status);
                }
            }
        );
    }
}

// ============================================================================
// MAIN LOOP: THE HEARTBEAT
// ============================================================================
/**
 * @brief Executes one tick of the trading logic. 
 * This function is called repeatedly by the main program loop.
 */
void TradingEngine::run_trading_cycle() {
    
    // 1. Safety Check: Ensure Market Data is Fresh
    if (!validate_market_data()) return;

    // 2. Status Logging: Print state every 5 seconds so user knows bot is alive
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_status_log_).count() >= 5) {
        log_status();
        last_status_log_ = now;
    }

    // 3. State Machine Switch
    // The bot performs different actions depending on its current state.
    switch (current_state_.load()) {
        
        case BotState::IDLE:
            // Waiting for a signal. If not closing a previous trade, look for entry.
            if (!waiting_for_close_) evaluate_entry_signal();
            break;

        case BotState::PLACING_ORDER:
        case BotState::CANCELLING:
            // We sent a request to Bybit and are waiting for confirmation.
            // Check if we've been waiting too long (Timeout).
            handle_timeout();
            break;

        case BotState::WORKING:
            // Order is on the book but not filled. Check if we need to chase price.
            monitor_working_order();
            break;

        case BotState::IN_POSITION:
            // We are in a trade. Check profit/loss targets.
            manage_open_position();
            break;

        case BotState::RECOVERING:
            // We just lost. Prepare the next Martingale bet.
            apply_martingale_recovery();
            break;
    }
}

// ============================================================================
// DATA VALIDATION: SAFETY FIRST
// ============================================================================
/**
 * @brief Checks if the Order Book contains valid, up-to-date data.
 * @return true if safe to trade, false otherwise.
 */
bool TradingEngine::validate_market_data() {
    if (symbol_.empty()) return false;
    if (!symbol_manager_.is_subscribed(symbol_)) return false;

    auto ob = orderbook_manager_.get(symbol_);
    if (!ob) return false;

    // 1. Check Update Count
    uint64_t current_update = ob->get_update_count();
    
    // Note: On Testnet, updates are rare. We allow "stale" data to keep testing logic running.
    // On Mainnet, you would uncomment the return false below to enforce freshness.
    if (current_update == last_orderbook_update_) {
        // return false; // Uncomment for Mainnet High-Frequency Safety
    }
    last_orderbook_update_ = current_update;

    // 2. Fetch Prices (Thread-Safe Access)
    double bid, ask, bid_qty, ask_qty;
    bool has_bid = ob->get_best_bid(bid, bid_qty);
    bool has_ask = ob->get_best_ask(ask, ask_qty);

    // 3. Check for Empty Book
    if (!has_bid || !has_ask) {
        static auto last_log = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_log).count() > 5) {
            std::cout << "⚠ Orderbook Empty (Waiting for liquidity)...\n";
            last_log = now;
        }
        return false;
    }

    // 4. Check for Valid Quantities
    if (bid_qty <= 0 || ask_qty <= 0) return false;

    // 5. Check for Crossed Market (Bid >= Ask) - Data Error
    if (bid >= ask - 0.01) {  
        static auto last_log = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_log).count() > 5) {
            std::cout << "⚠ Crossed/Tight Market (Data Invalid) - Pausing...\n";
            last_log = now;
        }
        return false;
    }
    
    return true; // Data is good!
}

// ============================================================================
// ENTRY STRATEGY: CALCULATE PRICE
// ============================================================================
/**
 * @brief Decides the price and direction for entering a new trade.
 */
void TradingEngine::evaluate_entry_signal() {
    auto ob = orderbook_manager_.get(symbol_);
    double best_bid, best_ask, dummy;
    
    // Re-fetch latest prices
    if (!ob->get_best_bid(best_bid, dummy) || !ob->get_best_ask(best_ask, dummy)) {
        return;
    }

    // STRATEGY: Aggressive Taker
    // We want to fill immediately.
    // If Shorting: Sell below Bid (Crossing the spread down).
    // If Longing: Buy above Ask (Crossing the spread up).
    // The +/- 500.0 buffer ensures we match even if price moves slightly.
    double price = is_short_ ? (best_bid - 5.0) : (best_ask + 5.0);
    
    place_order(price, is_short_);
}

// ============================================================================
// CHASE LOGIC: HANDLING STUCK ORDERS
// ============================================================================
/**
 * @brief Checks if our Limit Order is "left behind" by the market.
 * Only runs if state is WORKING.
 */
void TradingEngine::monitor_working_order() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - state_entry_time_).count();
    
    // Give the order at least 2 seconds to fill naturally before cancelling.
    if (elapsed < 2000) return;
    
    auto ob = orderbook_manager_.get(symbol_);
    double best_bid, best_ask, dummy;
    if (!ob->get_best_bid(best_bid, dummy) || !ob->get_best_ask(best_ask, dummy)) return;

    double chase_threshold = 10.0; // If price moves $10 away, we chase.
    bool chase_needed = false;

    if (!is_short_) { 
        // We are Buying. If Best Bid moves UP past our price, we are too low.
        if (best_bid > active_order_price_ + chase_threshold) {
            std::cout << "📉 Market moved up away from us. Chasing...\n";
            chase_needed = true;
        }
    } 
    else { 
        // We are Selling. If Best Ask moves DOWN past our price, we are too high.
        if (best_ask < active_order_price_ - chase_threshold) {
            std::cout << "📈 Market moved down away from us. Chasing...\n";
            chase_needed = true;
        }
    }

    if (chase_needed) {
        std::cout << "🔄 Cancelling order to re-place at better level...\n";
        if (trade_client_) {
            trade_client_->cancel_order(symbol_, active_order_id_);
            current_state_ = BotState::CANCELLING; // Lock state while cancelling
            state_entry_time_ = std::chrono::steady_clock::now();
        }
    }
}

// ============================================================================
// PNL MANAGEMENT: PROFIT & LOSS
// ============================================================================
/**
 * @brief Calculates current profit/loss percentage for the open position.
 * Closes position if Target or Stop Loss is hit.
 */
void TradingEngine::manage_open_position() {
    if (!position_filled_) return; // Sanity check

    auto ob = orderbook_manager_.get(symbol_);
    double best_bid, best_ask, dummy;
    if (!ob->get_best_bid(best_bid, dummy) || !ob->get_best_ask(best_ask, dummy)) return;

    // Calculate Exit Price
    // If Short, we exit by Buying at Ask. If Long, we exit by Selling at Bid.
    double current_price = is_short_ ? best_ask : best_bid;  
    double pnl_percent = 0.0;

    // Formula: (Exit - Entry) / Entry  [Reversed for shorts]
    if (is_short_) {
        pnl_percent = (entry_price_ - current_price) / entry_price_;
    } else {
        pnl_percent = (current_price - entry_price_) / entry_price_;
    }

    // Update stats for display
    last_pnl_percent_ = pnl_percent;
    last_pnl_dollars_ = pnl_percent * entry_price_ * current_qty_;

    // DECISION LOGIC
    if (pnl_percent >= profit_target_percent_) {
        std::cout << "\n✅ TARGET HIT! (+" << (pnl_percent * 100) << "%)\n";
        close_position_with_profit();
    }
    else if (pnl_percent <= stop_loss_percent_) {
        std::cout << "\n🛑 STOP LOSS! (" << (pnl_percent * 100) << "%)\n";
        
        // Check if we can still double down (Martingale)
        if (martingale_step_ < max_martingale_steps_) {
            close_position_with_loss();
        } else {
            // Too many losses in a row. Stop doubling.
            close_position_and_reset();
        }
    }
}

// ============================================================================
// CLOSE LOGIC: STATE TRANSITIONS
// ============================================================================
void TradingEngine::close_position_with_profit() {
    total_trades_++;
    winning_trades_++;
    total_profit_ += last_pnl_dollars_;
    
    close_position(); // Send the exit order
    
    // Reset Risk Parameters (Back to base size)
    martingale_step_ = 0;
    current_qty_ = base_quantity_;
    cumulative_loss_ = 0.0;
    print_statistics();
}

void TradingEngine::close_position_with_loss() {
    total_trades_++;
    cumulative_loss_ += std::abs(last_pnl_dollars_);
    total_profit_ += last_pnl_dollars_;
    
    close_position(); // Send the exit order
    
    // Set state to RECOVERING so we calculate the double-down next cycle
    current_state_ = BotState::RECOVERING;
}

void TradingEngine::close_position_and_reset() {
    total_trades_++;
    total_profit_ += last_pnl_dollars_;
    close_position();
    
    // Hard Reset (Giving up on this martingale sequence)
    martingale_step_ = 0;
    current_qty_ = base_quantity_;
    cumulative_loss_ = 0.0;
    std::cout << "⚠️ Max Steps Reached. Hard Resetting Risk.\n";
    print_statistics();
}

void TradingEngine::apply_martingale_recovery() {
    // This function runs AFTER a loss is closed
    martingale_step_++;
    current_qty_ *= 2.0;       // Double the size
    is_short_ = !is_short_;    // Reverse Strategy (If Long failed, go Short)
    
    std::cout << "⚡ MARTINGALE STEP " << martingale_step_ 
              << " | New Qty: " << current_qty_ 
              << " | Reversing to " << (is_short_ ? "SHORT" : "LONG") << "...\n";
              
    current_state_ = BotState::IDLE; // Ready to enter immediately
}

/**
 * @brief Sends the Market Order to exit the position.
 */
void TradingEngine::close_position() {
    if (!trade_client_) return;
    
    std::string side = is_short_ ? "Buy" : "Sell"; // To close Short, we Buy.
    auto ob = orderbook_manager_.get(symbol_);
    double best_bid, best_ask, dummy;
    if (!ob->get_best_bid(best_bid, dummy) || !ob->get_best_ask(best_ask, dummy)) {
        std::cerr << "❌ Cannot close - no market data\n";
        return;
    }
    
    // Aggressive Exit Price calculation
    double price = is_short_ ? best_ask + 100.0 : best_bid - 100.0;

    active_order_id_ = generate_id();
    waiting_for_close_ = true; // Flag tells OnOrderUpdate this is an EXIT
    
    // Lock state so we don't double-close
    current_state_ = BotState::PLACING_ORDER;
    state_entry_time_ = std::chrono::steady_clock::now();

    std::cout << "📤 CLOSING Position (" << side << " @ " << price 
              << ") Entry was: " << entry_price_ << "\n";
              
    trade_client_->place_order(symbol_, side, current_qty_, price, active_order_id_);
    
    // Clean up Aeron buffer
    if (aeron_publisher_) aeron_publisher_->remove_order_from_buffer(symbol_);
}

// ============================================================================
// EXECUTION: SENDING ORDERS
// ============================================================================
void TradingEngine::place_order(double price, bool is_short) {
    if (!trade_client_) return;

    active_order_id_ = generate_id();
    active_order_price_ = price; 
    std::string side = is_short ? "Sell" : "Buy";

    // Update State Variables
    current_state_ = BotState::PLACING_ORDER; // Critical: Prevents duplicate orders
    state_entry_time_ = std::chrono::steady_clock::now();
    entry_price_ = price;
    is_short_ = is_short;
    position_filled_ = false;

    std::cout << "📤 Sending " << side << " @ " << price << " (ID: " << active_order_id_ << ")\n";
    trade_client_->place_order(symbol_, side, current_qty_, price, active_order_id_);

    // SBE Logging (High Speed Binary Logging via Aeron)
    if (aeron_publisher_) {
        uint64_t now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        sbe_encoder_.encode_order(
            now_ns, active_order_id_, symbol_, side, price, current_qty_, true
        );
        aeron_publisher_->publish(sbe_encoder_.data(), sbe_encoder_.size());
    }
}

// ============================================================================
// ORDER UPDATE HANDLER: CALLBACK
// ============================================================================
/**
 * @brief Called by BybitWebSocketClient when an order status changes.
 */
void TradingEngine::on_order_update(const std::string& order_id, const std::string& status) {
    if (order_id != active_order_id_) {
        // Ignore updates for old/unknown orders
        return;
    }

    std::cout << "⚡ Update [" << order_id.substr(0, 15) << "...]: " << status << "\n";

    if (status == "New") {
        if (current_state_ == BotState::PLACING_ORDER) {
            std::cout << "  ↳ Order accepted, now working...\n";
            current_state_ = BotState::WORKING;
            state_entry_time_ = std::chrono::steady_clock::now();
        }
    }
    else if (status == "Filled") {
        // Check if this was an EXIT or an ENTRY
        if (waiting_for_close_) {
            std::cout << "✅ Exit Filled. Cycle Complete.\n";
            waiting_for_close_ = false;
            position_filled_ = false;
            
            // If we are recovering (after loss), stay in RECOVERING state.
            // If profitable, go to IDLE.
            if (current_state_ != BotState::RECOVERING) {
                current_state_ = BotState::IDLE;
            }
        } else {
            std::cout << "✅ Entry Filled. Monitoring PnL...\n";
            current_state_ = BotState::IN_POSITION;
            position_filled_ = true;
            position_entry_time_ = std::chrono::steady_clock::now();
        }
    }
    else if (status == "Cancelled") {
        std::cout << "🚫 Order Cancelled. Back to IDLE.\n";
        current_state_ = BotState::IDLE;
        waiting_for_close_ = false;
        position_filled_ = false;
    }
    else if (status == "Rejected") {
        std::cout << "❌ Order Rejected. Back to IDLE.\n";
        current_state_ = BotState::IDLE;
        waiting_for_close_ = false;
        position_filled_ = false;
    }
}

// ============================================================================
// UTILITIES
// ============================================================================
void TradingEngine::handle_timeout() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - state_entry_time_).count();
    
    // If waiting for confirmation > 5 seconds, cancel and reset.
    if (elapsed > ORDER_TIMEOUT_MS) {
        std::cerr << "⏰ Timeout (" << elapsed << "ms)! Forcing cancel...\n";
        if (trade_client_) {
            trade_client_->cancel_order(symbol_, active_order_id_);
        }
        state_entry_time_ = now;
    }
}

void TradingEngine::reconcile_state_on_startup() {
    // Check if Aeron memory has an active order from a previous crash
    if (aeron_publisher_ && aeron_publisher_->has_order_in_buffer(symbol_)) {
        auto rec = aeron_publisher_->get_order_from_buffer(symbol_);
        std::cout << "🔄 RECOVERING STATE from memory buffer...\n";
        std::cout << "  Order ID: " << rec.order_id << "\n";
        std::cout << "  Price: " << rec.price << " | Qty: " << rec.quantity << "\n";
        
        active_order_id_ = rec.order_id;
        current_qty_ = rec.quantity;
        is_short_ = (std::string(rec.side) == "Sell");
        entry_price_ = rec.price;
        position_filled_ = true;
        current_state_ = BotState::IN_POSITION;
    }
}

std::string TradingEngine::generate_id() {
    return "BOT_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}

void TradingEngine::print_statistics() {
    double win_rate = total_trades_ > 0 ? (double)winning_trades_ / total_trades_ * 100 : 0;
    std::cout << "📊 Stats: " << winning_trades_ << "/" << total_trades_ 
              << " Wins (" << std::fixed << std::setprecision(1) << win_rate << "%) "
              << "| Total PnL: $" << std::fixed << std::setprecision(2) << total_profit_ << "\n";
}

void TradingEngine::log_status() {
    std::cout << "💓 Heartbeat [" << symbol_ << "] State: ";
    switch (current_state_.load()) {
        case BotState::IDLE: std::cout << "IDLE"; break;
        case BotState::PLACING_ORDER: std::cout << "PLACING"; break;
        case BotState::WORKING: std::cout << "WORKING"; break;
        case BotState::IN_POSITION: std::cout << "IN_POSITION"; break;
        case BotState::CANCELLING: std::cout << "CANCELLING"; break;
        case BotState::RECOVERING: std::cout << "RECOVERING"; break;
    }
    
    if (position_filled_) {
        std::cout << " | PnL: " << std::fixed << std::setprecision(2) 
                  << (last_pnl_percent_ * 100) << "% ($" << last_pnl_dollars_ << ")";
    }
    std::cout << "\n";
}