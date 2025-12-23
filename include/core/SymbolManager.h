#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>

class SymbolManager {
public:
    explicit SymbolManager(const std::vector<std::string>& initial_symbols);
    
    // Delete copy operations
    SymbolManager(const SymbolManager&) = delete;
    SymbolManager& operator=(const SymbolManager&) = delete;
    
    // Allow move operations
    SymbolManager(SymbolManager&&) noexcept = default;
    SymbolManager& operator=(SymbolManager&&) noexcept = default;
    
    void add_symbol(const std::string& symbol);
    void mark_subscribed(const std::string& symbol);
    std::vector<std::string> get_symbols() const;
    std::vector<std::string> get_unsubscribed() const;
    size_t get_count() const;

private:
    std::vector<std::string> symbols_;
    std::unordered_map<std::string, bool> subscribed_;
    mutable std::mutex mutex_;
};