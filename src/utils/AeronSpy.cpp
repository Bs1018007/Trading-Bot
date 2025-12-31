// src/utils/AeronSpy.cpp
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include "Aeron.h"

using namespace aeron;

// Handle Ctrl+C to stop cleanly
std::atomic<bool> running(true);
void sig_handler(int) { running = false; }

// Callback function: This runs every time data is found in the buffer
fragment_handler_t handler = [](AtomicBuffer& buffer, util::index_t offset, util::index_t length, Header& header) {
    std::cout << "📨 RECEIVED [" << length << " bytes] from Session " << header.sessionId() << "\n";
    
    // Read the raw data as a string (assuming you sent text/json)
    // If you sent SBE binary, this will look like garbage characters, but it proves arrival.
    std::string data(reinterpret_cast<const char*>(buffer.buffer() + offset), length);
    std::cout << "   Payload: " << data.substr(0, 100) << (data.length() > 100 ? "..." : "") << "\n\n";
};

int main() {
    std::signal(SIGINT, sig_handler);

    std::cout << "🕵️‍♂️  STARTING AERON SPY...\n";
    std::cout << "    Channel: aeron:ipc\n";
    std::cout << "    Stream:  1001\n";

    // 1. Connect to the same Media Driver your bot uses
    Context context;
    std::shared_ptr<Aeron> aeron = Aeron::connect(context);

    // 2. Add a Subscriber to the specific channel
    std::int64_t id = aeron->addSubscription("aeron:ipc", 1001);
    std::shared_ptr<Subscription> subscription = aeron->findSubscription(id);

    // 3. Wait for the subscription to be ready
    while (!subscription) {
        std::this_thread::yield();
        subscription = aeron->findSubscription(id);
    }
    std::cout << "✅ Connected to Buffer! Waiting for data...\n";

    // 4. Constant loop to poll for new data
    while (running) {
        // poll(handler, limit) -> Returns number of fragments read
        int fragments_read = subscription->poll(handler, 10);
        if (fragments_read == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Don't burn 100% CPU waiting
        }
    }

    std::cout << "🛑 Spy stopping.\n";
    return 0;
}