#include "core/OrderBookManager.h"
#include <iostream>

// THREAD-SAFE: Finds the OrderBook for a symbol (e.g., "BTCUSDT").
// If it doesn't exist yet, it creates a new empty one, saves it, and returns it.
// Uses a lock to ensure two threads don't try to create the same book at the exact same time.
std::shared_ptr<OrderBook> OrderBookManager::get_or_create(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (orderbooks_.find(symbol) == orderbooks_.end()) {
        orderbooks_[symbol] = std::make_shared<OrderBook>();
        std::cout << "✓ Created orderbook for: " << symbol << "\n";
    }
    return orderbooks_[symbol];
}

// THREAD-SAFE: strict lookup.
// Returns the pointer to the OrderBook if it exists.
// Returns nullptr (null) if we haven't created an orderbook for this symbol yet.
// Useful when trading logic needs to check data but shouldn't be creating new memory.
std::shared_ptr<OrderBook> OrderBookManager::get(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = orderbooks_.find(symbol);
    if (it != orderbooks_.end()) {
        return it->second;
    }
    return nullptr;
}

// THREAD-SAFE: Snapshots the entire collection.
// Returns a copy of the map containing ALL symbols and their OrderBooks currently in memory.
// Useful for loops that need to iterate through every active symbol (e.g., printing a dashboard).
std::unordered_map<std::string, std::shared_ptr<OrderBook>> OrderBookManager::get_all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return orderbooks_;
}

// THREAD-SAFE: Returns the count of active OrderBooks.
// Simply tells you how many symbols we are currently tracking/subscribed to.
size_t OrderBookManager::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return orderbooks_.size();
}