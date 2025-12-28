#include "messaging/SBEEncoder.h"
#include <cstring>

// ============================================================================
// CORE FUNCTIONS
// ============================================================================
void SBEEncoder::reset() {
    offset_ = 0;
}

char* SBEEncoder::data() {
    return buffer_.data();
}

size_t SBEEncoder::size() const {
    return offset_;
}

// ============================================================================
// ENCODER: Orderbook Snapshot (Template ID 2)
// ============================================================================
void SBEEncoder::encode_orderbook_snapshot(
    uint64_t timestamp,
    const std::vector<std::pair<double, double>>& bids,
    const std::vector<std::pair<double, double>>& asks,
    const std::string& symbol
) {
    reset();
    
    // Header
    write_uint16(48); // BlockLength
    write_uint16(2);  // TemplateId
    write_uint16(1);  // SchemaId
    write_uint16(0);  // Version
    
    // Body
    write_uint64(timestamp);
    write_uint16(static_cast<uint16_t>(bids.size()));
    write_uint16(static_cast<uint16_t>(asks.size()));
    
    // Bids Group
    write_uint16(16); // BlockLength
    write_uint16(static_cast<uint16_t>(bids.size()));
    for (const auto& [price, qty] : bids) {
        write_double(price);
        write_double(qty);
    }
    
    // Asks Group
    write_uint16(16); // BlockLength
    write_uint16(static_cast<uint16_t>(asks.size()));
    for (const auto& [price, qty] : asks) {
        write_double(price);
        write_double(qty);
    }
    
    // Var Data
    write_string(symbol);
}

// ============================================================================
// ENCODER: Trade Signal (Template ID 3)
// ============================================================================
void SBEEncoder::encode_trade_signal(
    uint64_t timestamp,
    uint8_t action,
    double price,
    double quantity,
    const std::string& symbol
) {
    reset();
    
    // Header
    write_uint16(32); // BlockLength
    write_uint16(3);  // TemplateId
    write_uint16(1);  // SchemaId
    write_uint16(0);  // Version
    
    // Body
    write_uint64(timestamp);
    write_uint8(action);
    write_double(price);
    write_double(quantity);
    
    // Var Data
    write_string(symbol);
}

// ============================================================================
// [NEW] ENCODER: Active Order (Template ID 4)
// This fixes the "no member named encode_order" error
// ============================================================================
void SBEEncoder::encode_order(
    uint64_t timestamp,
    const std::string& order_id,
    const std::string& symbol,
    const std::string& side,
    double price,
    double quantity,
    bool is_active
) {
    reset();

    // Message Header
    write_uint16(64); // BlockLength (Approximation of fixed fields)
    write_uint16(4);  // TemplateId (4 = Order)
    write_uint16(1);  // SchemaId
    write_uint16(0);  // Version

    // Fixed Body Fields
    write_uint64(timestamp);
    write_double(price);
    write_double(quantity);
    write_uint8(is_active ? 1 : 0);

    // Variable Length Strings (Order must match decoder expectation)
    write_string(order_id);
    write_string(symbol);
    write_string(side);
}

// ============================================================================
// LOW-LEVEL WRITERS
// ============================================================================

void SBEEncoder::write_uint8(uint8_t value) {
    if (offset_ + 1 > buffer_.size()) buffer_.resize(buffer_.size() * 2);
    buffer_[offset_++] = static_cast<char>(value);
}

void SBEEncoder::write_uint16(uint16_t value) {
    if (offset_ + 2 > buffer_.size()) buffer_.resize(buffer_.size() * 2);
    std::memcpy(buffer_.data() + offset_, &value, sizeof(value));
    offset_ += sizeof(value);
}

void SBEEncoder::write_uint64(uint64_t value) {
    if (offset_ + 8 > buffer_.size()) buffer_.resize(buffer_.size() * 2);
    std::memcpy(buffer_.data() + offset_, &value, sizeof(value));
    offset_ += sizeof(value);
}

void SBEEncoder::write_double(double value) {
    if (offset_ + 8 > buffer_.size()) buffer_.resize(buffer_.size() * 2);
    std::memcpy(buffer_.data() + offset_, &value, sizeof(value));
    offset_ += sizeof(value);
}

// Helper to write length + string data
void SBEEncoder::write_string(const std::string& str) {
    uint16_t len = static_cast<uint16_t>(str.length());
    write_uint16(len);
    
    if (offset_ + len > buffer_.size()) buffer_.resize(buffer_.size() * 2);
    std::memcpy(buffer_.data() + offset_, str.data(), len);
    offset_ += len;
}