#pragma once
#include <Aeron.h>
#include <memory>
#include <atomic>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <mutex>

// Order stored in Aeron buffer
struct AeronOrderRecord {
    std::string order_id;
    std::string symbol;
    double price;
    double quantity;
    std::string side;
    uint64_t timestamp;
    bool is_active;
};

class AeronPublisher {
public:
    AeronPublisher(const std::string& channel, int32_t stream_id);
    
    bool init();
    
    // ✨ NEW: Order-specific publish methods
    bool publish_order(const AeronOrderRecord& order);
    bool publish_orderbook(const char* buffer, size_t length);
    
    // ✨ NEW: Order buffer management
    bool has_order_in_buffer(const std::string& symbol) const;
    AeronOrderRecord get_order_from_buffer(const std::string& symbol) const;
    void remove_order_from_buffer(const std::string& symbol);
    void update_order_in_buffer(const std::string& symbol, const AeronOrderRecord& order);
    
    // ✨ NEW: Get all orders
    std::unordered_map<std::string, AeronOrderRecord> get_all_orders() const;
    
    // Original methods
    bool publish(const char* buffer, size_t length);
    bool is_connected() const;
    uint64_t get_messages_sent() const;

private:
    std::shared_ptr<aeron::Aeron> aeron_;
    std::shared_ptr<aeron::Publication> publication_;
    std::string channel_;
    int32_t stream_id_;
    std::atomic<uint64_t> messages_sent_{0};
    std::atomic<uint64_t> offer_failures_{0};
    
    // ✨ NEW: In-memory order buffer (simulates Aeron buffer)
    mutable std::mutex buffer_mutex_;
    std::unordered_map<std::string, AeronOrderRecord> order_buffer_;
    
    // Helper to serialize order
    std::string serialize_order(const AeronOrderRecord& order);
};