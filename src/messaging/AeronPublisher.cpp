#include "messaging/AeronPublisher.h"
#include "messaging/GlobalMediaDriver.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>

AeronPublisher::AeronPublisher(const std::string& channel, int32_t stream_id)
    : channel_(channel), stream_id_(stream_id) {}

bool AeronPublisher::init() {
    try {
        if (!GlobalMediaDriver::get_instance().initialize()) {
            return false;
        }

        aeron::Context context;
        context.mediaDriverTimeout(5000);
        aeron_ = aeron::Aeron::connect(context);

        if (!aeron_) {
            std::cerr << "Failed to connect to Aeron\n";
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Aeron initialization error: " << e.what() << "\n";
        return false;
    }

    int64_t pub_id = aeron_->addPublication(channel_, stream_id_);

    for (int i = 0; i < 100; ++i) {
        publication_ = aeron_->findPublication(pub_id);
        if (publication_) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (!publication_) {
        std::cerr << "Failed to create Aeron publication: "
                  << channel_ << " stream " << stream_id_ << "\n";
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::cout << "✓ Aeron publisher ready: "
              << channel_ << " stream " << stream_id_ 
              << " (connected: " << (publication_->isConnected() ? "YES" : "NO") << ")\n";

    if (!publication_->isConnected()) {
        std::cout << "  ⚠ No subscribers detected (buffer mode active)\n";
    }

    return true;
}

// ============================================================================
// ✨ NEW: Publish order and save to buffer
// ============================================================================
bool AeronPublisher::publish_order(const AeronOrderRecord& order) {
    std::cout << "📤 Publishing order to Aeron buffer: " << order.order_id << "\n";
    
    // Serialize order to string
    std::string serialized = serialize_order(order);
    
    // Try to publish to Aeron
    bool published = publish(serialized.c_str(), serialized.length());
    
    // Always save to in-memory buffer (our local copy)
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        order_buffer_[order.symbol] = order;
        std::cout << "  ✓ Saved to buffer: " << order.symbol 
                  << " (total orders: " << order_buffer_.size() << ")\n";
    }
    
    return published;
}

// ============================================================================
// ✨ NEW: Publish orderbook data
// ============================================================================
bool AeronPublisher::publish_orderbook(const char* buffer, size_t length) {
    return publish(buffer, length);
}

// ============================================================================
// ✨ NEW: Check if order exists in buffer
// ============================================================================
bool AeronPublisher::has_order_in_buffer(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    auto it = order_buffer_.find(symbol);
    return (it != order_buffer_.end() && it->second.is_active);
}

// ============================================================================
// ✨ NEW: Get order from buffer
// ============================================================================
AeronOrderRecord AeronPublisher::get_order_from_buffer(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    auto it = order_buffer_.find(symbol);
    if (it != order_buffer_.end()) {
        return it->second;
    }
    return AeronOrderRecord{};  // Return empty if not found
}

// ============================================================================
// ✨ NEW: Remove order from buffer
// ============================================================================
void AeronPublisher::remove_order_from_buffer(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    auto it = order_buffer_.find(symbol);
    if (it != order_buffer_.end()) {
        it->second.is_active = false;
        std::cout << "  ✓ Removed order from buffer: " << symbol << "\n";
    }
}

// ============================================================================
// ✨ NEW: Update order in buffer
// ============================================================================
void AeronPublisher::update_order_in_buffer(const std::string& symbol, const AeronOrderRecord& order) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    order_buffer_[symbol] = order;
    std::cout << "  ✓ Updated order in buffer: " << symbol << "\n";
}

// ============================================================================
// ✨ NEW: Get all orders
// ============================================================================
std::unordered_map<std::string, AeronOrderRecord> AeronPublisher::get_all_orders() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return order_buffer_;
}

// ============================================================================
// Original publish method
// ============================================================================
bool AeronPublisher::publish(const char* buffer, size_t length) {
    if (!publication_) return false;

    const auto result = publication_->offer(
        aeron::concurrent::AtomicBuffer(
            reinterpret_cast<std::uint8_t*>(const_cast<char*>(buffer)),
            static_cast<std::int32_t>(length)));

    if (result > 0) {
        messages_sent_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    offer_failures_.fetch_add(1, std::memory_order_relaxed);
    return false;
}

bool AeronPublisher::is_connected() const {
    return publication_ && publication_->isConnected();
}

uint64_t AeronPublisher::get_messages_sent() const {
    return messages_sent_.load(std::memory_order_relaxed);
}

// ============================================================================
// Helper: Serialize order to string
// ============================================================================
std::string AeronPublisher::serialize_order(const AeronOrderRecord& order) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(8);
    ss << "ORDER|"
       << order.order_id << "|"
       << order.symbol << "|"
       << order.price << "|"
       << order.quantity << "|"
       << order.side << "|"
       << order.timestamp << "|"
       << (order.is_active ? "1" : "0");
    
    return ss.str();
}