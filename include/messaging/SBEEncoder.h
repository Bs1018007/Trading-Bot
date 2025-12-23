#pragma once
#include <array>
#include <vector>
#include <string>
#include <cstdint>

class SBEEncoder {
public:
    void reset();
    char* data();
    size_t size() const;
    
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

private:
    std::array<char, 4096> buffer_;
    size_t offset_ = 0;
    
    void write_uint8(uint8_t value);
    void write_uint16(uint16_t value);
    void write_uint64(uint64_t value);
    void write_double(double value);
};