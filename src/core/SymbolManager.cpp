#include "core/SymbolManager.h"
#include <iostream>

// THREAD-SAFE: Adds a unique symbol (e.g., "BTCUSDT") to the subscription list.
// Returns TRUE if the symbol was actually added (it was new).
// Returns FALSE if the symbol was already in the list (prevents duplicates).
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

// THREAD-SAFE: Checks if we are currently watching a specific symbol.
// Returns true if the symbol exists in our set, false otherwise.
// Used before trading to ensure we actually have data for this coin.
bool SymbolManager::is_subscribed(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return subscribed_symbols_.find(symbol) != subscribed_symbols_.end();
}

// THREAD-SAFE: Returns a snapshot of ALL symbols we are tracking.
// Converts the internal set into a standard vector string list.
// Useful when you need to iterate over every coin (e.g., to unsubscribe from everything).
std::vector<std::string>SymbolManager::get_all_symbols() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<std::string>(subscribed_symbols_.begin(), subscribed_symbols_.end());
}

// THREAD-SAFE: Returns the total number of unique symbols currently subscribed.
size_t SymbolManager::get_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return subscribed_symbols_.size();
}