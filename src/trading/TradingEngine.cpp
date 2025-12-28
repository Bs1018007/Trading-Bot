#include "trading/TradingEngine.h"
#include <iostream>
#include <cmath>
#include <thread>
#include <cstring>
#include <iomanip>

// ============================================================================
// CONSTRUCTOR
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
    std::cout << "\n╔════════════════════════════════════════════════════╗\n";
    std::cout << "║   🎲 MARTINGALE CHASER v3.1 (SBE Enabled) 🎲      ║\n";
    std::cout << "╚════════════════════════════════════════════════════╝\n";
    std::cout << "Strategy:         Best Bid/Ask + Chase Logic\n";
    std::cout << "Encoding:         SBE (Simple Binary Encoding)\n";
    std::cout << "Profit Target:    0.05%\n";
    std::cout << "Stop Loss:        -0.10%\n\n";

    // Init Params
    base_quantity_ = 0.001;
    current_qty_ = base_quantity_;
    martingale_step_ = 0;
    max_martingale_steps_ = 6;
    profit_target_percent_ = 0.0005;
    stop_loss_percent_ = -0.001;
    cumulative_loss_ = 0.0;
    
    is_short_ = false;
    position_filled_ = false;
    waiting_for_close_ = false;

    reconcile_state_on_startup();

    if (trade_client_) {
        trade_client_->set_order_update_callback(
            [this](const std::string& id, const std::string& status, const std::string& sym) {
                if (sym == symbol_ || (sym.empty() && id == active_order_id_)) {
                    this->on_order_update(id, status);
                }
            }
        );
    }
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void TradingEngine::run_trading_cycle() {
    
    if (!validate_market_data()) return;

    // Heartbeat Log
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_status_log_).count() >= 5) {
        log_status();
        last_status_log_ = now;
    }

    switch (current_state_.load()) {
        
        case BotState::IDLE:
            if (!waiting_for_close_) evaluate_entry_signal();
            break;

        case BotState::PLACING_ORDER:
        case BotState::CANCELLING:
            handle_timeout();
            break;

        case BotState::WORKING:
            // Check if price moved away. If so, cancel and re-place.
            monitor_working_order();
            break;

        case BotState::IN_POSITION:
            manage_open_position();
            break;

        case BotState::RECOVERING:
            apply_martingale_recovery();
            break;
    }
}

// ============================================================================
// DATA VALIDATION
// ============================================================================
bool TradingEngine::validate_market_data() {
    if (symbol_.empty()) return false;
    if (!symbol_manager_.is_subscribed(symbol_)) return false;

    auto ob = orderbook_manager_.get(symbol_);
    if (!ob) return false;

    double bid, ask, dummy;
    if (!ob->get_best_bid(bid, dummy) || !ob->get_best_ask(ask, dummy)) return false;

    if (bid >= ask) {
        static auto last_log = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_log).count() > 5) {
            std::cout << "⚠ Crossed market (Bid=" << bid << " >= Ask=" << ask << ") - Pausing...\n";
            last_log = now;
        }
        return false;
    }
    return true;
}

// ============================================================================
// ENTRY STRATEGY
// ============================================================================
void TradingEngine::evaluate_entry_signal() {
    auto ob = orderbook_manager_.get(symbol_);
    double best_bid, best_ask, dummy;
    ob->get_best_bid(best_bid, dummy);
    ob->get_best_ask(best_ask, dummy);

    double price;
    
    // STRATEGY: Place order slightly inside the spread to be first
    if (is_short_) {
        // Sell at Best Ask - 0.01 (Front run other sellers)
        price = best_ask - 0.01;
        std::cout << "\n🔴 OPENING SHORT | Qty: " << current_qty_ << " | Price: " << price << "\n";
    } else {
        // Buy at Best Bid + 0.01 (Front run other buyers)
        price = best_bid + 0.01;
        std::cout << "\n🟢 OPENING LONG  | Qty: " << current_qty_ << " | Price: " << price << "\n";
    }

    place_order(price, is_short_);
}

// ============================================================================
// CHASE LOGIC
// ============================================================================
void TradingEngine::monitor_working_order() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - state_entry_time_).count();
    
    // Don't chase until the order has been alive for at least 500ms
    if (elapsed < 500) return;
    auto ob = orderbook_manager_.get(symbol_);
    double best_bid, best_ask, dummy;
    ob->get_best_bid(best_bid, dummy);
    ob->get_best_ask(best_ask, dummy);

    // How far can price move before we cancel?
    double chase_threshold = 50.0; // $50 tolerance
    bool chase_needed = false;

    if (!is_short_) { 
        // BUYING
        if (best_bid > active_order_price_ + chase_threshold) {
            std::cout << "📉 Market moved up to " << best_bid << ". Chasing...\n";
            chase_needed = true;
        }
    } 
    else { 
        // SELLING
        if (best_ask < active_order_price_ - chase_threshold) {
            std::cout << "📈 Market moved down to " << best_ask << ". Chasing...\n";
            chase_needed = true;
        }
    }

    if (chase_needed) {
        std::cout << "🔄 Cancelling stuck order to re-place...\n";
        trade_client_->cancel_order(symbol_, active_order_id_);
        current_state_ = BotState::CANCELLING;
        state_entry_time_ = std::chrono::steady_clock::now();
    }
}

// ============================================================================
// PNL MANAGEMENT
// ============================================================================
void TradingEngine::manage_open_position() {
    if (!position_filled_) return;

    auto ob = orderbook_manager_.get(symbol_);
    double best_bid, best_ask, dummy;
    ob->get_best_bid(best_bid, dummy);
    ob->get_best_ask(best_ask, dummy);

    double current_price = is_short_ ? best_ask : best_bid;
    double pnl_percent = 0.0;

    if (is_short_) {
        pnl_percent = (entry_price_ - current_price) / entry_price_;
    } else {
        pnl_percent = (current_price - entry_price_) / entry_price_;
    }

    last_pnl_percent_ = pnl_percent;
    last_pnl_dollars_ = pnl_percent * entry_price_ * current_qty_;

    if (pnl_percent >= profit_target_percent_) {
        std::cout << "\n✅ TARGET HIT! (+" << (pnl_percent * 100) << "%)\n";
        close_position_with_profit();
    }
    else if (pnl_percent <= stop_loss_percent_) {
        std::cout << "\n🛑 STOP LOSS! (" << (pnl_percent * 100) << "%)\n";
        if (martingale_step_ < max_martingale_steps_) {
            close_position_with_loss();
        } else {
            close_position_and_reset();
        }
    }
}

// ============================================================================
// CLOSE LOGIC
// ============================================================================
void TradingEngine::close_position_with_profit() {
    total_trades_++;
    winning_trades_++;
    total_profit_ += last_pnl_dollars_;
    close_position();
    martingale_step_ = 0;
    current_qty_ = base_quantity_;
    cumulative_loss_ = 0.0;
    print_statistics();
}

void TradingEngine::close_position_with_loss() {
    total_trades_++;
    cumulative_loss_ += std::abs(last_pnl_dollars_);
    total_profit_ += last_pnl_dollars_;
    close_position();
    current_state_ = BotState::RECOVERING;
}

void TradingEngine::close_position_and_reset() {
    total_trades_++;
    total_profit_ += last_pnl_dollars_;
    close_position();
    martingale_step_ = 0;
    current_qty_ = base_quantity_;
    cumulative_loss_ = 0.0;
    std::cout << "⚠️ Max Steps. Hard Reset.\n";
    print_statistics();
}

void TradingEngine::apply_martingale_recovery() {
    martingale_step_++;
    current_qty_ *= 2.0;
    is_short_ = !is_short_;
    std::cout << "⚡ MARTINGALE STEP " << martingale_step_ << " | Qty: " << current_qty_ << " | Reversing...\n";
    current_state_ = BotState::IDLE;
}

void TradingEngine::close_position() {
    if (!trade_client_) return;
    
    // Close aggressively (Market Taker)
    std::string side = is_short_ ? "Buy" : "Sell";
    auto ob = orderbook_manager_.get(symbol_);
    double best_bid, best_ask, dummy;
    ob->get_best_bid(best_bid, dummy);
    ob->get_best_ask(best_ask, dummy);
    
    // Ensure fill for close
    double price = is_short_ ? best_ask * 1.001 : best_bid * 0.999;

    active_order_id_ = generate_id();
    waiting_for_close_ = true;
    current_state_ = BotState::PLACING_ORDER;
    state_entry_time_ = std::chrono::steady_clock::now();

    std::cout << "📤 CLOSING Position (" << side << " @ " << price << ")\n";
    trade_client_->place_order(symbol_, side, current_qty_, price, active_order_id_);
    
    if (aeron_publisher_) aeron_publisher_->remove_order_from_buffer(symbol_);
}

// ============================================================================
// EXECUTION WITH SBE
// ============================================================================
void TradingEngine::place_order(double price, bool is_short) {
    if (!trade_client_) return;

    active_order_id_ = generate_id();
    active_order_price_ = price; 
    std::string side = is_short ? "Sell" : "Buy";

    current_state_ = BotState::PLACING_ORDER;
    state_entry_time_ = std::chrono::steady_clock::now();
    entry_price_ = price;
    is_short_ = is_short;
    position_filled_ = false;

    std::cout << "📤 Sending " << side << " @ " << price << "\n";
    trade_client_->place_order(symbol_, side, current_qty_, price, active_order_id_);

    // [UPDATED] SBE ENCODING
    // Instead of sending a raw C-Struct, we encode using SBE and publish the binary blob.
    if (aeron_publisher_) {
        // 1. Generate Timestamp
        uint64_t now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // 2. Encode to SBE
        sbe_encoder_.encode_order(
            now_ns,
            active_order_id_,
            symbol_,
            side,
            price,
            current_qty_,
            true // is_active
        );

        // 3. Publish Binary Data (data + size)
        aeron_publisher_->publish(sbe_encoder_.data(), sbe_encoder_.size());
    }
}

void TradingEngine::on_order_update(const std::string& order_id, const std::string& status) {
    if (order_id != active_order_id_) return;

    std::cout << "⚡ Update: " << status << "\n";

    if (status == "New") {
        current_state_ = BotState::WORKING;
    }
    else if (status == "Filled") {
        if (waiting_for_close_) {
            std::cout << "✅ Exit Filled. Profit/Loss Booked.\n";
            waiting_for_close_ = false;
            position_filled_ = false;
            current_state_ = BotState::IDLE;
        } else {
            std::cout << "✅ Entry Filled. Now monitoring PnL...\n";
            current_state_ = BotState::IN_POSITION;
            position_filled_ = true;
            position_entry_time_ = std::chrono::steady_clock::now();
        }
    }
    else if (status == "Cancelled") {
        std::cout << "🚫 Order Cancelled. Resetting to IDLE to re-evaluate.\n";
        current_state_ = BotState::IDLE;
        waiting_for_close_ = false;
    }
    else if (status == "Rejected") {
        // [CRITICAL FIX] If cancel failed, the order is likely FILLED. 
        // Stop the cancel loop and check PnL.
        if (current_state_ == BotState::CANCELLING) {
            std::cout << "⚠️ Cancel Rejected (Likely Filled). Moving to Position State.\n";
            current_state_ = BotState::IN_POSITION;
            position_filled_ = true;
            position_entry_time_ = std::chrono::steady_clock::now();
        } else {
            std::cout << "❌ Order Rejected. Resetting.\n";
            current_state_ = BotState::IDLE;
        }
    }
}

void TradingEngine::handle_timeout() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - state_entry_time_).count() > ORDER_TIMEOUT_MS) {
        std::cerr << "⏰ Timeout! Cancelling...\n";
        trade_client_->cancel_order(symbol_, active_order_id_);
        state_entry_time_ = now;
    }
}

void TradingEngine::reconcile_state_on_startup() {
    if (aeron_publisher_ && aeron_publisher_->has_order_in_buffer(symbol_)) {
        // Note: For full SBE support, AeronPublisher needs to decode SBE data here.
        // Assuming legacy support or updated publisher:
        auto rec = aeron_publisher_->get_order_from_buffer(symbol_);
        std::cout << "🔄 RECOVERING...\n";
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
    std::cout << "📊 Stats: " << winning_trades_ << "/" << total_trades_ << " Wins | Total PnL: $" << total_profit_ << "\n";
}

void TradingEngine::log_status() {
    if (position_filled_) {
        std::cout << "💹 PnL: " << (last_pnl_percent_ * 100) << "% ($" << last_pnl_dollars_ << ")\n";
    }
}