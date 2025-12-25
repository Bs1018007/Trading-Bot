#pragma once
#include <Aeron.h>
#include <memory>
#include <atomic>
#include <string>
#include <cstdint>

class AeronPublisher {
public:
    AeronPublisher(const std::string& channel, int32_t stream_id);
    
    bool init();
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
};