#include "core/SymbolManager.h"
#include <iostream>

bool SymbolManager::add_symbol(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto result = subscribed_symbols_.insert(symbol);
    
    if (result.second) {  // New symbol added
        std::cout << "✓ Added new symbol: " << symbol 
                  << " (total: " << subscribed_symbols_.size() << ")\n";
        return true;
    }
    
    return false;  // Already exists
}

bool SymbolManager::is_subscribed(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return subscribed_symbols_.find(symbol) != subscribed_symbols_.end();
}

std::vector<std::string>SymbolManager::get_all_symbols() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<std::string>(subscribed_symbols_.begin(), subscribed_symbols_.end());
}


size_t SymbolManager::get_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return subscribed_symbols_.size();
}