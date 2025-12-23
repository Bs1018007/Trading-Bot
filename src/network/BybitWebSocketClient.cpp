#include "network/BybitWebSocketClient.h"
#include <iostream>
#include <cstring>
#include <chrono>

struct lws_protocols BybitWebSocketClient::protocols_[] = {
    { "bybit-protocol", BybitWebSocketClient::callback_function, 0, 65536 },
    { NULL, NULL, 0, 0 }
};

BybitWebSocketClient::BybitWebSocketClient(
    OrderBookManager& obm,
    SymbolManager& sm,
    BotConfiguration& config,
    DataLogger& logger
) : orderbook_manager_(obm), symbol_manager_(sm), config_(config), data_logger_(logger) {
    
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols_;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.user = this;
    
    context_ = lws_create_context(&info);
    if (!context_) {
        throw std::runtime_error("Failed to create WebSocket context");
    }
    
    if (config_.enable_aeron) {
        aeron_pub_ = std::make_unique<AeronPublisher>(
            config_.aeron_channel, config_.orderbook_stream_id);
        
        if (!aeron_pub_->init()) {
            std::cerr << "⚠ Aeron disabled - continuing without IPC\n";
            aeron_pub_.reset();
        }
    }
}

BybitWebSocketClient::~BybitWebSocketClient() {
    running_ = false;
    if (context_) {
        lws_context_destroy(context_);
    }
}

int BybitWebSocketClient::callback_function(
    struct lws* wsi,
    enum lws_callback_reasons reason,
    void* user,
    void* in,
    size_t len
) {
    BybitWebSocketClient* client = static_cast<BybitWebSocketClient*>(
        lws_context_user(lws_get_context(wsi)));
    
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            std::cout << "✓ WebSocket connected\n";
            client->subscribe_orderbooks();
            break;
            
        case LWS_CALLBACK_CLIENT_RECEIVE: {
            char* msg_data = static_cast<char*>(in);
            std::string msg_str(msg_data, len);
            static int msg_count = 0;
            if (msg_count++ % 10000 == 0) {
                std::cout << "\n📩 WebSocket JSON (Message #" << msg_count << "):\n";
                if (len > 1000) {
                    std::cout << msg_str.substr(0, 1000) << "...\n\n";
                } else {
                    std::cout << msg_str << "\n\n";
                }
            }
            client->handle_message(msg_data, len);
            break;
        }
            
        case LWS_CALLBACK_CLIENT_CLOSED:
            std::cout << "✗ WebSocket closed\n";
            break;
            
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            std::cerr << "✗ Connection error\n";
            break;
            
        default:
            break;
    }
    return 0;
}

void BybitWebSocketClient::subscribe_orderbooks() {
    auto symbols = symbol_manager_.get_symbols();
    
    const int MAX_SYMBOLS_PER_CONNECTION = 200;
    
    std::string args = "[";
    int count = 0;
    
    for (size_t i = 0; i < symbols.size(); ++i) {
        args += "\"orderbook.50." + symbols[i] + "\"";
        if (i < symbols.size() - 1) args += ",";
        
        count++;
        
        if (count >= MAX_SYMBOLS_PER_CONNECTION && i < symbols.size() - 1) {
            args += "]";
            
            std::string sub_msg = "{\"op\":\"subscribe\",\"args\":" + args + "}";
            unsigned char buf[LWS_PRE + 8192];
            memcpy(&buf[LWS_PRE], sub_msg.c_str(), sub_msg.length());
            lws_write(wsi_, &buf[LWS_PRE], sub_msg.length(), LWS_WRITE_TEXT);
            
            std::cout << "✓ Subscribed to batch " << (i / MAX_SYMBOLS_PER_CONNECTION + 1) 
                     << " (" << count << " symbols)\n";
            
            args = "[";
            count = 0;
        }
    }
    
    if (count > 0) {
        args += "]";
        std::string sub_msg = "{\"op\":\"subscribe\",\"args\":" + args + "}";
        unsigned char buf[LWS_PRE + 8192];
        memcpy(&buf[LWS_PRE], sub_msg.c_str(), sub_msg.length());
        lws_write(wsi_, &buf[LWS_PRE], sub_msg.length(), LWS_WRITE_TEXT);
        
        std::cout << "✓ Subscribed to final batch (" << count << " symbols)\n";
    }
    
    std::cout << "✓ Total subscribed: " << symbols.size() << " symbols\n\n";
    for (const auto& sym : symbols) {
        symbol_manager_.mark_subscribed(sym);
    }
}

void BybitWebSocketClient::handle_message(char* data, size_t len) {
    try {
        simdjson::padded_string padded(data, len);
        simdjson::ondemand::document doc = parser_.iterate(padded);
        
        auto topic_result = doc["topic"];
        if (topic_result.error()) return;
        
        std::string_view topic_str = topic_result.get_string().value();
        
        size_t last_dot = topic_str.rfind('.');
        if (last_dot == std::string::npos) return;
        
        std::string symbol(topic_str.substr(last_dot + 1));
        
        auto orderbook = orderbook_manager_.get_or_create(symbol);
        auto data_obj = doc["data"].get_object();
        
        std::vector<PriceLevel> bids;
        auto bids_array = data_obj["b"].get_array();
        for (auto bid : bids_array) {
            auto bid_array = bid.get_array();
            auto it = bid_array.begin();
            std::string_view price_sv = (*it).get_string().value();
            ++it;
            std::string_view qty_sv = (*it).get_string().value();
            
            bids.push_back({
                std::stod(std::string(price_sv)),
                std::stod(std::string(qty_sv))
            });
        }
        
        std::vector<PriceLevel> asks;
        auto asks_array = data_obj["a"].get_array();
        for (auto ask : asks_array) {
            auto ask_array = ask.get_array();
            auto it = ask_array.begin();
            std::string_view price_sv = (*it).get_string().value();
            ++it;
            std::string_view qty_sv = (*it).get_string().value();
            
            asks.push_back({
                std::stod(std::string(price_sv)),
                std::stod(std::string(qty_sv))
            });
        }
        
        orderbook->update_bids(bids);
        orderbook->update_asks(asks);
        orderbook->increment_update();
        
        // Log orderbook data
        auto bids_log = orderbook->get_bids(5);
        auto asks_log = orderbook->get_asks(5);
        double mid_price = 0.0;
        if (!bids_log.empty() && !asks_log.empty()) {
            mid_price = (bids_log[0].first + asks_log[0].first) / 2.0;
        }
        data_logger_.log_orderbook(symbol, mid_price, bids_log, asks_log);
        
        if (config_.enable_aeron && aeron_pub_ && aeron_pub_->is_connected()) {
            auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            auto bids_vec = orderbook->get_bids(10);
            auto asks_vec = orderbook->get_asks(10);
            
            sbe_encoder_.encode_orderbook_snapshot(
                timestamp, bids_vec, asks_vec, symbol);
            
            if (aeron_pub_->publish(sbe_encoder_.data(), sbe_encoder_.size())) {
                aeron_published_.fetch_add(1, std::memory_order_relaxed);
            }
        }
        
        messages_received_.fetch_add(1, std::memory_order_relaxed);
        
        if (messages_received_ % 500 == 0) {
            std::cout << "Total updates: " << messages_received_ 
                     << " | Active symbols: " << orderbook_manager_.size()
                     << " | Aeron: " << aeron_published_ << "\n";
        }
        
    } catch (const simdjson::simdjson_error&) {
        // Ignore parse errors
    }
}

void BybitWebSocketClient::connect() {
    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    
    ccinfo.context = context_;
    ccinfo.address = "stream.bybit.com";
    ccinfo.port = 443;
    ccinfo.path = "/v5/public/linear";
    ccinfo.host = ccinfo.address;
    ccinfo.origin = ccinfo.address;
    ccinfo.protocol = protocols_[0].name;
    ccinfo.ssl_connection = LCCSCF_USE_SSL;
    
    wsi_ = lws_client_connect_via_info(&ccinfo);
    if (!wsi_) {
        throw std::runtime_error("Failed to connect to WebSocket");
    }
}

void BybitWebSocketClient::run() {
    while (running_) {
        lws_service(context_, 50);
    }
}

void BybitWebSocketClient::stop() {
    running_ = false;
}

uint64_t BybitWebSocketClient::get_message_count() const {
    return messages_received_.load();
}

uint64_t BybitWebSocketClient::get_aeron_count() const {
    return aeron_published_.load();
}