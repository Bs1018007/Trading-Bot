#pragma once
#include "core/OrderBook.h"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <string>

class OrderBookManager {
public:
    std::shared_ptr<OrderBook> get_or_create(const std::string& symbol);
    std::shared_ptr<OrderBook> get(const std::string& symbol) const;
    std::unordered_map<std::string, std::shared_ptr<OrderBook>> get_all() const;
    size_t size() const;

private:
    std::unordered_map<std::string, std::shared_ptr<OrderBook>> orderbooks_;
    mutable std::mutex mutex_;
};