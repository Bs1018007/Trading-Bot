#include "messaging/SBEEncoder.h"
#include <cstring>

void SBEEncoder::reset() {
    offset_ = 0;
}

char* SBEEncoder::data() {
    return buffer_.data();
}

size_t SBEEncoder::size() const {
    return offset_;
}

void SBEEncoder::encode_orderbook_snapshot(
    uint64_t timestamp,
    const std::vector<std::pair<double, double>>& bids,
    const std::vector<std::pair<double, double>>& asks,
    const std::string& symbol
) {
    reset();
    
    // Message header
    write_uint16(48);                                    // blockLength
    write_uint16(2);                                     // templateId
    write_uint16(1);                                     // schemaId
    write_uint16(0);                                     // version
    
    // Message body
    write_uint64(timestamp);
    write_uint16(static_cast<uint16_t>(bids.size()));
    write_uint16(static_cast<uint16_t>(asks.size()));
    
    // Bids group
    write_uint16(16);                                    // blockLength
    write_uint16(static_cast<uint16_t>(bids.size()));   // numInGroup
    for (const auto& [price, qty] : bids) {
        write_double(price);
        write_double(qty);
    }
    
    // Asks group
    write_uint16(16);                                    // blockLength
    write_uint16(static_cast<uint16_t>(asks.size()));   // numInGroup
    for (const auto& [price, qty] : asks) {
        write_double(price);
        write_double(qty);
    }
    
    // Symbol (variable length)
    write_uint16(static_cast<uint16_t>(symbol.length()));
    std::memcpy(buffer_.data() + offset_, symbol.data(), symbol.length());
    offset_ += symbol.length();
}

void SBEEncoder::encode_trade_signal(
    uint64_t timestamp,
    uint8_t action,
    double price,
    double quantity,
    const std::string& symbol
) {
    reset();
    
    // Message header
    write_uint16(32);                                    // blockLength
    write_uint16(3);                                     // templateId
    write_uint16(1);                                     // schemaId
    write_uint16(0);                                     // version
    
    // Message body
    write_uint64(timestamp);
    write_uint8(action);
    write_double(price);
    write_double(quantity);
    
    // Symbol (variable length)
    write_uint16(static_cast<uint16_t>(symbol.length()));
    std::memcpy(buffer_.data() + offset_, symbol.data(), symbol.length());
    offset_ += symbol.length();
}

void SBEEncoder::write_uint8(uint8_t value) {
    buffer_[offset_++] = static_cast<char>(value);
}

void SBEEncoder::write_uint16(uint16_t value) {
    std::memcpy(buffer_.data() + offset_, &value, sizeof(value));
    offset_ += sizeof(value);
}

void SBEEncoder::write_uint64(uint64_t value) {
    std::memcpy(buffer_.data() + offset_, &value, sizeof(value));
    offset_ += sizeof(value);
}

void SBEEncoder::write_double(double value) {
    std::memcpy(buffer_.data() + offset_, &value, sizeof(value));
    offset_ += sizeof(value);
}
