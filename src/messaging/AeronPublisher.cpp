#include "messaging/AeronPublisher.h"
#include "messaging/GlobalMediaDriver.h"
#include <iostream>
#include <thread>
#include <chrono>

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
        std::cout << "  ⚠ No subscribers detected\n";
    }

    return true;
}

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
