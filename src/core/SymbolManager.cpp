#include "core/SymbolManager.h"
#include <iostream>

SymbolManager::SymbolManager(const std::vector<std::string>& initial_symbols)
    : symbols_(initial_symbols) {
    for (const auto& sym : symbols_) {
        subscribed_[sym] = false;
    }
}

void SymbolManager::add_symbol(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (subscribed_.find(symbol) == subscribed_.end()) {
        symbols_.push_back(symbol);
        subscribed_[symbol] = false;
        std::cout << "✓ Added symbol: " << symbol << "\n";
    }
}

void SymbolManager::mark_subscribed(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscribed_[symbol] = true;
}

std::vector<std::string> SymbolManager::get_symbols() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return symbols_;
}

std::vector<std::string> SymbolManager::get_unsubscribed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> unsubscribed;
    for (const auto& sym : symbols_) {
        auto it = subscribed_.find(sym);
        if (it != subscribed_.end() && !it->second) {
            unsubscribed.push_back(sym);
        }
    }
    return unsubscribed;
}

size_t SymbolManager::get_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return symbols_.size();
}
