#pragma once
#include <vector>
#include <string>
#include <unordered_set>
#include <mutex>

class SymbolManager {
public:
    SymbolManager() = default;
    
    // Add symbol if not already tracked
    bool add_symbol(const std::string& symbol);
    
    // Check if symbol is subscribed
    bool is_subscribed(const std::string& symbol) const;
    
    // Get all subscribed symbols
    std::vector<std::string> get_all_symbols() const;
    
    // Get count
    size_t get_count() const;

private:
    std::unordered_set<std::string> subscribed_symbols_;
    mutable std::mutex mutex_;
};