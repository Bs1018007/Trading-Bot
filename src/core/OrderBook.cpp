#include "core/OrderBook.h"
#include <algorithm>
// It basically replaces the bid side with new bid levels(snapshots),overwrites the old data,publishes update safely using atomics
void OrderBook::update_bids(const std::vector<PriceLevel>& bids) {
    int count = std::min(static_cast<int>(bids.size()), MAX_LEVELS);
    for (int i = 0; i < count; i++) {
        bids_[i] = bids[i];
    }
    bid_count_.store(count, std::memory_order_release);
}
// It basically replaces the ask side with new ask levels(snapshots),overwrites the old data,publishes update safely using atomics
void OrderBook::update_asks(const std::vector<PriceLevel>& asks) {
    int count = std::min(static_cast<int>(asks.size()), MAX_LEVELS);
    for (int i = 0; i < count; i++) {
        asks_[i] = asks[i];
    }
    ask_count_.store(count, std::memory_order_release);
}
// Returns the highest bid price and quantity.
bool OrderBook::get_best_bid(double& price, double& qty) const {
    int count = bid_count_.load(std::memory_order_acquire);
    if (count > 0) {
        price = bids_[0].price;
        qty = bids_[0].quantity;
        return true;
    }
    return false;
}
// Returns the lowest ask price and quantity.
bool OrderBook::get_best_ask(double& price, double& qty) const {
    int count = ask_count_.load(std::memory_order_acquire);
    if (count > 0) {
        price = asks_[0].price;
        qty = asks_[0].quantity;
        return true;
    }
    return false;
}
//Calculates the fair-price. that is best bid and best ask divided by 2
double OrderBook::get_fair_price() const {
    double bid_price, ask_price, bid_qty, ask_qty;
    if (get_best_bid(bid_price, bid_qty) && get_best_ask(ask_price, ask_qty)) {
        return (bid_price + ask_price) / 2.0;
    }
    return 0.0;
}
// Returns a copy of bid levels (price, quantity).Used for logging/UI.Not for low-latency trading path
std::vector<std::pair<double, double>> OrderBook::get_bids(int max_levels) const {
    std::vector<std::pair<double, double>> result;
    int count = std::min(bid_count_.load(std::memory_order_acquire), max_levels);
    for (int i = 0; i < count; i++) {
        result.push_back({bids_[i].price, bids_[i].quantity});
    }
    return result;
}
//Returns a copy of ask levels (price, quantity).Used for logging/UI.Not for low-latency trading path.
std::vector<std::pair<double, double>> OrderBook::get_asks(int max_levels) const {
    std::vector<std::pair<double, double>> result;
    int count = std::min(ask_count_.load(std::memory_order_acquire), max_levels);
    for (int i = 0; i < count; i++) {
        result.push_back({asks_[i].price, asks_[i].quantity});
    }
    return result;
}

// Atomically increments the order book’s version counter to mark that a new logical market data update has been applied, enabling lock-free monitoring and synchronization.
void OrderBook::increment_update() {
    update_id_.fetch_add(1, std::memory_order_relaxed);
}

// Returns the current order book version number, allowing consumers to quickly detect fresh versus stale market data with a lock-free atomic read.
uint64_t OrderBook::get_update_count() const {
    return update_id_.load(std::memory_order_relaxed);
}
