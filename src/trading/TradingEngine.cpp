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
    base_quantity_ = 0.01;          // Starting trade size (e.g., 0.001 BTC)
    current_qty_ = base_quantity_;   // Current trade size (will double on loss)
    martingale_step_ = 0;            // Tracks consecutive losses
    max_martingale_steps_ = 100000;       // Stop doubling after 6 losses to prevent blow-up
    profit_target_percent_ = 0.001; // 0.05% gain target
    stop_loss_percent_ = -0.0005;     // -0.05% loss limit
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
    if (bid > ask ) {  
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
// In src/trading/TradingEngine.cpp

void TradingEngine::evaluate_entry_signal() {
    auto ob = orderbook_manager_.get(symbol_);
    double best_bid, best_ask, dummy;
    if (!ob->get_best_bid(best_bid, dummy) || !ob->get_best_ask(best_ask, dummy)) return;

    double price = 0.0;
    
    // STRATEGY: Mid-Market (Faster Fills for Maker)
    // We calculate the middle of the spread.
    double mid_price = (best_bid + best_ask) / 2.0;

    if (!is_short_) {
        // Buying: Offer slightly above middle
        price = mid_price - 0.1; 
        if (price >= best_ask) price = best_ask - 0.005; // Safety cap
    } else {
        // Selling: Offer slightly below middle
        price = mid_price + 0.1;
        if (price <= best_bid) price = best_bid + 0.005; // Safety cap
    }
    
    std::cout << "🤝 MID-MARKET ENTRY: Placing order at " << price << "\n";
    
    // Still using Maker (PostOnly) to save fees
    place_order(price, is_short_, true); 
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
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - state_entry_time_).count();

    // 1. TIME LIMIT CHECK (The Fix)
    // If the order is older than 3000ms (3 seconds) and hasn't filled, CANCEL IT.
    // This forces the bot to "wake up" and re-evaluate the price.
    if (elapsed_ms > 10000) {
        std::cout << "⏰ Order stale (" << elapsed_ms << "ms). Cancelling to refresh...\n";
        if (trade_client_) {
            trade_client_->cancel_order(symbol_, active_order_id_);
            current_state_ = BotState::CANCELLING;
            state_entry_time_ = now;
        }
        return; 
    }
    
    // 2. PRICE DRIFT CHECK (Existing Logic)
    // Only run this if the order is "young" (less than 3 seconds)
    if (elapsed_ms < 500) return; // Wait 0.5s before checking price

    auto ob = orderbook_manager_.get(symbol_);
    double best_bid, best_ask, dummy;
    if (!ob->get_best_bid(best_bid, dummy) || !ob->get_best_ask(best_ask, dummy)) return;

    bool chase_needed = false;

    if (!is_short_) { 
        // Buying: If Best Bid moves ABOVE us, we are losing.
        // Make this sensitive: If we are beaten by even $0.05, chase.
        if (best_bid > active_order_price_ + 0.05) { 
            std::cout << "📉 Lost Top Spot! (Bid: " << best_bid << "). Chasing...\n";
            chase_needed = true;
        }
    } 
    else { 
        // Selling: If Best Ask moves BELOW us, we are losing.
        if (best_ask < active_order_price_ - 0.05) {
            std::cout << "📈 Lost Top Spot! (Ask: " << best_ask << "). Chasing...\n";
            chase_needed = true;
        }
    }

    if (chase_needed) {
        if (trade_client_) {
            trade_client_->cancel_order(symbol_, active_order_id_);
            current_state_ = BotState::CANCELLING; 
            state_entry_time_ = now;
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
    if (!position_filled_) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - state_entry_time_).count();
    if (elapsed < 500) return; // Wait 1 second for market to settle
    // 1. Get Market Data
    auto ob = orderbook_manager_.get(symbol_);
    double best_bid, best_ask, dummy;
    if (!ob->get_best_bid(best_bid, dummy) || !ob->get_best_ask(best_ask, dummy)) return;

    double current_market_price = is_short_ ? best_ask : best_bid;  
    
    // 2. Calculate PnL
    double pnl_percent = 0.0;
    if (is_short_) {
        pnl_percent = (entry_price_ - current_market_price) / entry_price_;
    } else {
        pnl_percent = (current_market_price - entry_price_) / entry_price_;
    }

    last_pnl_percent_ = pnl_percent;
    last_pnl_dollars_ = pnl_percent * entry_price_ * current_qty_;

    // 3. CHECK STOP LOSS
    if (pnl_percent <= stop_loss_percent_) {
        std::cout << "\n🛑 STOP LOSS HIT: " << (pnl_percent * 100) << "% (Price: " << current_market_price << ")\n";

        // A. Cancel the resting Profit Order first (Unlock the coins)
        if (trade_client_ && !active_exit_order_id_.empty()) {
            std::cout << "  ⚡ Cancelling Profit Order (" << active_exit_order_id_ << ") to execute Stop Loss...\n";
            trade_client_->cancel_order(symbol_, active_exit_order_id_);
            active_exit_order_id_ = ""; 
        }

        
        std::cout << "  🔄 PREPARING REVERSAL: Closing now, will Double & Reverse on fill.\n";
             
        // Set a flag so on_order_update knows to reverse
        trigger_martingale_on_close_ = true; 
             
        close_position(); // Send Market Exit
    }
}
/*void TradingEngine::execute_average_down(double current_market_price) {
    if (!trade_client_) return;

    // 1. Calculate how much to add (Doubling: Add equal to current holdings)
    // Example: Hold 0.01 -> Buy 0.01 -> Total 0.02
    // Example: Hold 0.02 -> Buy 0.02 -> Total 0.04
    double quantity_to_add = current_qty_; 

    // 2. Prepare Order
    active_order_id_ = generate_id();
    std::string side = is_short_ ? "Sell" : "Buy"; // SAME DIRECTION
    
    // 3. Set Flags
    is_averaging_ = true;             // Tell system we are adding, not entering new
    current_state_ = BotState::PLACING_ORDER;
    state_entry_time_ = std::chrono::steady_clock::now();
    active_order_price_ = current_market_price; // Track for chase logic if needed

    std::cout << "➕ AVERAGING: Adding " << quantity_to_add << " " << side 
              << " @ " << current_market_price << "\n";

    // 4. Send Taker Order (Fill Immediately)
    // Using current_market_price ensures immediate fill (Taker)
    trade_client_->place_order(symbol_, side, quantity_to_add, current_market_price, active_order_id_, false);
}*/

// ============================================================================
// CLOSE LOGIC: STATE TRANSITIONS
// ============================================================================
/*void TradingEngine::close_position_with_profit() {
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
    
    std::cout << "📉 Trade Lost. Resetting to Base Size (No Martingale).\n";
    current_qty_ = base_quantity_; // Reset size to 0.01 (or whatever base is)
    martingale_step_ = 0;          // Reset step count
    current_state_ = BotState::IDLE;
}*/

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
              
    trade_client_->place_order(symbol_, side, current_qty_, price, active_order_id_,false);
    
    // Clean up Aeron buffer
    if (aeron_publisher_) aeron_publisher_->remove_order_from_buffer(symbol_);
}

// ============================================================================
// EXECUTION: SENDING ORDERS
// ============================================================================
void TradingEngine::place_order(double price, bool is_short,bool is_maker) {
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
    trade_client_->place_order(symbol_, side, current_qty_, price, active_order_id_,is_maker);

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
// ============================================================================
// ORDER UPDATE HANDLER: CALLBACK
// ============================================================================
/**
 * @brief Called by BybitWebSocketClient when an order status changes.
 * IMPLEMENTS: Stop-and-Reverse Martingale & Instant Exit Posting
 */
void TradingEngine::on_order_update(const std::string& order_id, const std::string& status) {
    // 1. START TIMER (Measure Latency correctly)
    auto start_time = std::chrono::high_resolution_clock::now();

    // 2. Ignore updates for orders we don't care about
    // We check both the active entry ID and the active exit ID
    if (order_id != active_order_id_ && order_id != active_exit_order_id_) {
        return;
    }

    std::cout << "⚡ Update [" << order_id.substr(0, 15) << "...]: " << status << "\n";

    // ---------------------------------------------------------
    // STATUS: NEW (Order Accepted)
    // ---------------------------------------------------------
    if (status == "New") {
        if (current_state_ == BotState::PLACING_ORDER && order_id == active_order_id_) {
            std::cout << "  ↳ Order accepted, now working...\n";
            current_state_ = BotState::WORKING;
            state_entry_time_ = std::chrono::steady_clock::now();
        }
    }
    // ---------------------------------------------------------
    // STATUS: FILLED (Trade Executed)
    // ---------------------------------------------------------
    else if (status == "Filled") {
        
        // CASE A: We just CLOSED a position (Profit Take or Stop Loss)
        if (waiting_for_close_) {
            std::cout << "✅ Position Closed.\n";
            
            // Clear flags
            waiting_for_close_ = false;
            position_filled_ = false;
            active_exit_order_id_ = ""; 

            // LOGIC: Check if this was a Stop Loss that requires Reversal
            if (trigger_martingale_on_close_) {
                std::cout << "⚡ LOSS REALIZED. Triggering Stop-and-Reverse...\n";
                
                // 1. Martingale Math: Flip Side, Double Size
                martingale_step_++;
                current_qty_ *= 2.0;       // Double the size
                is_short_ = !is_short_;    // Flip Direction (Long <-> Short)
                
                // 2. Reset Trigger Flag
                trigger_martingale_on_close_ = false;

                // 3. Set State to IDLE 
                // This forces the main loop to call evaluate_entry_signal() IMMEDIATELY,
                // which will place the new reversed order at the new size.
                current_state_ = BotState::IDLE; 
                
                std::cout << "🚀 REVERSING: New Qty " << current_qty_ 
                          << " | Direction: " << (is_short_ ? "SHORT" : "LONG") << "\n";
            } 
            else {
                // Standard Profit Take -> Reset to Base Risk
                std::cout << "💰 Cycle Complete (Profit). Resetting to Base Size.\n";
                martingale_step_ = 0;
                current_qty_ = base_quantity_; 
                current_state_ = BotState::IDLE;
            }
        }
        
        // CASE B: We just OPENED a position (Entry Filled)
        // Action: Post the Exit Order IMMEDIATELY
        else if (order_id == active_order_id_) {

            if (position_filled_) return;
            std::cout << "✅ Entry Filled. Placing Exit Order IMMEDIATELY...\n";
            position_filled_ = true;
            current_state_ = BotState::IN_POSITION;

            // 1. Calculate Target Price (Take Profit)
            double target_price;
            if (is_short_) {
                // If Short, buy back lower
                target_price = entry_price_ * (1.0 - profit_target_percent_);
            } else {
                // If Long, sell higher
                target_price = entry_price_ * (1.0 + profit_target_percent_);
            }

            // 2. Generate ID and Send the Exit Order NOW
            std::string exit_side = is_short_ ? "Buy" : "Sell";
            active_exit_order_id_ = generate_id(); 

            std::cout << "⚡ POSTING EXIT: " << exit_side << " @ " << target_price << "\n";
            
            // Pass 'true' for Maker (PostOnly) to ensure we get paid for liquidity
            if (trade_client_) {
                trade_client_->place_order(symbol_, exit_side, current_qty_, target_price, active_exit_order_id_, true);
            }
        }
    }
    // ---------------------------------------------------------
    // STATUS: CANCELLED / REJECTED
    // ---------------------------------------------------------
    else if (status == "Cancelled" || status == "Rejected") {
        std::cout << "🚫 Order " << status << ".\n";
        
        // If the Exit Order was cancelled (e.g., manually or by Stop Loss logic), we are still in position
        if (order_id == active_exit_order_id_) {
             std::cout << "  ↳ Exit order cancelled. Position is UNLOCKED.\n";
             active_exit_order_id_ = "";
             // We stay IN_POSITION so manage_open_position can execute the Stop Loss market order
             current_state_ = BotState::IN_POSITION; 
        }
        // If the Closing (Market) order failed
        else if (waiting_for_close_) {
             std::cout << "  ↳ Critical: Closing order failed. Retrying...\n";
             // Stay in position, loop will retry close
             current_state_ = BotState::IN_POSITION;
             waiting_for_close_ = false;
        }
        // If the Entry order failed
        else {
            std::cout << "  ↳ Entry failed. Back to IDLE.\n";
            current_state_ = BotState::IDLE;
            position_filled_ = false;
        }
    }

    // 4. STOP TIMER (Measure actual processing time)
    auto end_time = std::chrono::high_resolution_clock::now();

    // 5. CALCULATE IN NANOSECONDS
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();

    std::cout << "⚡ Internal Logic Latency: " << duration << " nanoseconds\n";
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