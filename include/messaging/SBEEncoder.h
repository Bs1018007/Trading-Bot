#pragma once

#include <vector>
#include <string>
#include <cstdint>

class SBEEncoder {
public:
    SBEEncoder() {
        buffer_.resize(1024); // Allocate 1KB buffer
    }

    void reset();
    char* data();
    size_t size() const;

    // Existing methods
    void encode_orderbook_snapshot(
        uint64_t timestamp,
        const std::vector<std::pair<double, double>>& bids,
        const std::vector<std::pair<double, double>>& asks,
        const std::string& symbol
    );

    void encode_trade_signal(
        uint64_t timestamp,
        uint8_t action,
        double price,
        double quantity,
        const std::string& symbol
    );

    // [NEW] This is the function missing in your error!
    void encode_order(
        uint64_t timestamp,
        const std::string& order_id,
        const std::string& symbol,
        const std::string& side,
        double price,
        double quantity,
        bool is_active
    );

private:
    std::vector<char> buffer_;
    size_t offset_ = 0;

    void write_uint8(uint8_t value);
    void write_uint16(uint16_t value);
    void write_uint64(uint64_t value);
    void write_double(double value);
    void write_string(const std::string& str); // Helper for string writing
};