#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include <functional> // Required for std::function
#include <libwebsockets.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

#include "core/OrderBookManager.h"
#include "core/SymbolManager.h"
#include "config/BotConfiguration.h"
#include "utils/DataLogger.h"
#include "messaging/AeronPublisher.h"
#include "messaging/SBEEncoder.h"
#include "simdjson.h"

class BybitWebSocketClient {
public:
    enum class ChannelType {
        PUBLIC,
        PRIVATE_TRADE,
        PRIVATE_STREAM
    };

    // Callback Definition
    using OrderUpdateCallback = std::function<void(const std::string&, const std::string&, const std::string&)>;

    BybitWebSocketClient(
        OrderBookManager& obm,
        SymbolManager& sm,
        BotConfiguration& config,
        DataLogger& logger,
        ChannelType type = ChannelType::PUBLIC
    );
    
    ~BybitWebSocketClient();

    void connect();
    void run();
    void stop();
    bool is_connected() const { return connected_; }

    // Market Data
    void subscribe_to_symbol(const std::string& symbol);
    void subscribe_to_private_topics();
    
    // Trading Execution
    void authenticate();
    void place_order(const std::string& symbol, const std::string& side, 
                     double qty, double price, const std::string& order_link_id,bool is_maker);
    void cancel_order(const std::string& symbol, const std::string& order_link_id);

    // [CRITICAL FIX] 
    // This setter MUST use 'on_order_update_' to match the .cpp file
    void set_order_update_callback(OrderUpdateCallback cb) {
        on_order_update_ = cb; 
    }

    struct lws* get_wsi() const { return wsi_; }
    uint64_t get_message_count() const;
    uint64_t get_aeron_count() const;

    static int callback_function(struct lws* wsi, enum lws_callback_reasons reason, 
                               void* user, void* in, size_t len);

private:
    OrderBookManager& orderbook_manager_;
    SymbolManager& symbol_manager_;
    BotConfiguration& config_;
    DataLogger& data_logger_;
    
    ChannelType channel_type_;
    struct lws_context* context_ = nullptr;
    struct lws* wsi_ = nullptr;
    std::atomic<bool> running_{true};
    std::atomic<bool> connected_{false};
    
    std::string api_key_;
    std::string api_secret_;

    simdjson::ondemand::parser parser_;
    std::unique_ptr<AeronPublisher> aeron_pub_;
    SBEEncoder sbe_encoder_;
    std::atomic<uint64_t> messages_received_{0};
    std::atomic<uint64_t> aeron_published_{0};

    // [THE FIX IS HERE]
    // Renamed from 'order_update_callback_' to 'on_order_update_'
    OrderUpdateCallback on_order_update_;

    std::string generate_signature(long long expires);
    void handle_message(char* data, size_t len);
    void handle_order_update(char* data, size_t len);
    
    static struct lws_protocols protocols_[];
};