#pragma once
#include "core/OrderBookManager.h"
#include "core/SymbolManager.h"
#include "config/BotConfiguration.h"
#include "messaging/AeronPublisher.h"
#include "messaging/SBEEncoder.h"
#include "utils/DataLogger.h"
#include <libwebsockets.h>
#include <simdjson.h>
#include <atomic>
#include <memory>

class BybitWebSocketClient {
public:
    BybitWebSocketClient(
        OrderBookManager& obm,
        SymbolManager& sm,
        BotConfiguration& config,
        DataLogger& logger
    );
    ~BybitWebSocketClient();
    
    void connect();
    void run();
    void stop();
    void subscribe_to_symbol(const std::string& symbol);
    struct lws* get_wsi() const { return wsi_; }
    
    using OrderUpdateCallback = std::function<void(const std::string& order_id, 
                                                    const std::string& status,
                                                    const std::string& symbol)>;
    
    void set_order_update_callback(OrderUpdateCallback callback) {
        order_update_callback_ = callback;
    }
    
    uint64_t get_message_count() const;
    uint64_t get_aeron_count() const;

private:
    struct lws_context* context_;
    struct lws* wsi_;
    OrderBookManager& orderbook_manager_;
    SymbolManager& symbol_manager_;
    BotConfiguration& config_;
    DataLogger& data_logger_;
    simdjson::ondemand::parser parser_;
    std::unique_ptr<AeronPublisher> aeron_pub_;
    SBEEncoder sbe_encoder_;
    std::atomic<bool> running_{true};
    std::atomic<uint64_t> messages_received_{0};
    std::atomic<uint64_t> aeron_published_{0};
    std::vector<std::string> subscription_messages_;  // For batch subscriptions (currently unused)
    OrderUpdateCallback order_update_callback_;
    
    void handle_order_update(char* data, size_t len);
    
    static int callback_function(
        struct lws* wsi,
        enum lws_callback_reasons reason,
        void* user,
        void* in,
        size_t len
    );
    
    void subscribe_orderbooks();  // For batch subscriptions (currently unused)
    void handle_message(char* data, size_t len);
    // prepare_subscription_messages() removed - not needed for single symbol
    
    static struct lws_protocols protocols_[];
};