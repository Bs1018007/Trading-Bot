#include "core/OrderBook.h"
#include <algorithm>
#include <cstring>

// ============================================================================
// UPDATE METHODS (FIXED WITH PROPER ORDERING)
// ============================================================================

// [FIX] Ensure atomic visibility of updates
void OrderBook::update_bids(const std::vector<PriceLevel>& bids) {
    int count = std::min(static_cast<int>(bids.size()), MAX_LEVELS);
    
    // Copy data first
    for (int i = 0; i < count; i++) {
        bids_[i] = bids[i];
    }
    
    // [FIX] Ensure data is written before count is updated
    std::atomic_thread_fence(std::memory_order_release);
    
    // Update count (makes data visible to readers)
    bid_count_.store(count, std::memory_order_release);
}

void OrderBook::update_asks(const std::vector<PriceLevel>& asks) {
    int count = std::min(static_cast<int>(asks.size()), MAX_LEVELS);
    
    // Copy data first
    for (int i = 0; i < count; i++) {
        asks_[i] = asks[i];
    }
    
    // [FIX] Ensure data is written before count is updated
    std::atomic_thread_fence(std::memory_order_release);
    
    // Update count (makes data visible to readers)
    ask_count_.store(count, std::memory_order_release);
}

// ============================================================================
// GETTER METHODS (FIXED WITH ACQUIRE SEMANTICS)
// ============================================================================

bool OrderBook::get_best_bid(double& price, double& qty) const {
    // [FIX] Acquire count first to ensure we see updated data
    int count = bid_count_.load(std::memory_order_acquire);
    
    if (count > 0) {
        // [FIX] Ensure we read data after count
        std::atomic_thread_fence(std::memory_order_acquire);
        
        price = bids_[0].price;
        qty = bids_[0].quantity;
        
        // [FIX] Sanity check
        if (price <= 0 || qty <= 0) {
            return false;
        }
        return true;
    }
    return false;
}

bool OrderBook::get_best_ask(double& price, double& qty) const {
    // [FIX] Acquire count first to ensure we see updated data
    int count = ask_count_.load(std::memory_order_acquire);
    
    if (count > 0) {
        // [FIX] Ensure we read data after count
        std::atomic_thread_fence(std::memory_order_acquire);
        
        price = asks_[0].price;
        qty = asks_[0].quantity;
        
        // [FIX] Sanity check
        if (price <= 0 || qty <= 0) {
            return false;
        }
        return true;
    }
    return false;
}

double OrderBook::get_fair_price() const {
    double bid_price, ask_price, bid_qty, ask_qty;
    if (get_best_bid(bid_price, bid_qty) && get_best_ask(ask_price, ask_qty)) {
        // [FIX] Validate spread before returning
        if (bid_price < ask_price) {
            return (bid_price + ask_price) / 2.0;
        }
    }
    return 0.0;
}

// ============================================================================
// SNAPSHOT METHODS
// ============================================================================

std::vector<std::pair<double, double>> OrderBook::get_bids(int max_levels) const {
    std::vector<std::pair<double, double>> result;
    int count = std::min(bid_count_.load(std::memory_order_acquire), max_levels);
    
    std::atomic_thread_fence(std::memory_order_acquire);
    
    for (int i = 0; i < count; i++) {
        // [FIX] Skip invalid levels
        if (bids_[i].price > 0 && bids_[i].quantity > 0) {
            result.push_back({bids_[i].price, bids_[i].quantity});
        }
    }
    return result;
}

std::vector<std::pair<double, double>> OrderBook::get_asks(int max_levels) const {
    std::vector<std::pair<double, double>> result;
    int count = std::min(ask_count_.load(std::memory_order_acquire), max_levels);
    
    std::atomic_thread_fence(std::memory_order_acquire);
    
    for (int i = 0; i < count; i++) {
        // [FIX] Skip invalid levels
        if (asks_[i].price > 0 && asks_[i].quantity > 0) {
            result.push_back({asks_[i].price, asks_[i].quantity});
        }
    }
    return result;
}

// ============================================================================
// VERSION TRACKING
// ============================================================================

void OrderBook::increment_update() {
    update_id_.fetch_add(1, std::memory_order_relaxed);
}

uint64_t OrderBook::get_update_count() const {
    return update_id_.load(std::memory_order_relaxed);
}

