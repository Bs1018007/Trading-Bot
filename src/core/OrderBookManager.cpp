#include "core/OrderBookManager.h"
#include <iostream>

std::shared_ptr<OrderBook> OrderBookManager::get_or_create(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (orderbooks_.find(symbol) == orderbooks_.end()) {
        orderbooks_[symbol] = std::make_shared<OrderBook>();
        std::cout << "✓ Created orderbook for: " << symbol << "\n";
    }
    return orderbooks_[symbol];
}

std::shared_ptr<OrderBook> OrderBookManager::get(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = orderbooks_.find(symbol);
    if (it != orderbooks_.end()) {
        return it->second;
    }
    return nullptr;
}

std::unordered_map<std::string, std::shared_ptr<OrderBook>> OrderBookManager::get_all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return orderbooks_;
}

size_t OrderBookManager::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return orderbooks_.size();
}