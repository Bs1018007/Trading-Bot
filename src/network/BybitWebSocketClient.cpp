#include "network/BybitWebSocketClient.h"
#include <iostream>
#include <cstring>
#include <chrono>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <thread>

// ============================================================================
// INTERNAL STRUCTURES
// ============================================================================

struct SessionData {
    std::string rx_buffer;
};

struct lws_protocols BybitWebSocketClient::protocols_[] = {
    { "bybit-protocol", BybitWebSocketClient::callback_function, sizeof(SessionData), 65536 },
    { NULL, NULL, 0, 0 }
};

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

BybitWebSocketClient::BybitWebSocketClient(
    OrderBookManager& obm,
    SymbolManager& sm,
    BotConfiguration& config,
    DataLogger& logger,
    ChannelType type
) : orderbook_manager_(obm), 
    symbol_manager_(sm), 
    config_(config), 
    data_logger_(logger),
    channel_type_(type) 
{
    if (channel_type_ == ChannelType::PRIVATE_TRADE) {
        api_key_ = config_.api_key;
        api_secret_ = config_.api_secret;

        if (api_key_.empty() || api_secret_.empty()) {
            std::cerr << "⚠️  CRITICAL: Private Channel initialized without API Keys!\n";
        }
    }

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
    
    if (config_.enable_aeron && channel_type_ == ChannelType::PUBLIC) {
        aeron_pub_ = std::make_unique<AeronPublisher>(
            config_.aeron_channel, config_.orderbook_stream_id);
        
        if (!aeron_pub_->init()) {
            std::cerr << "⚠ Aeron disabled - continuing without IPC\n";
        }
    }
}

BybitWebSocketClient::~BybitWebSocketClient() {
    stop();
    if (context_) {
        lws_context_destroy(context_);
    }
}

// ============================================================================
// CONNECTION MANAGEMENT
// ============================================================================

void BybitWebSocketClient::connect() {
    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    
    ccinfo.context = context_;
    ccinfo.address = "stream-testnet.bybit.com"; 
    // ccinfo.address = "stream.bybit.com"; // Uncomment for MAINNET

    ccinfo.port = 443;
    ccinfo.host = ccinfo.address;
    ccinfo.origin = ccinfo.address;
    ccinfo.protocol = protocols_[0].name;
    ccinfo.ssl_connection = LCCSCF_USE_SSL;

    if (channel_type_ == ChannelType::PUBLIC) {
        ccinfo.path = "/v5/public/linear";
    } else {
        ccinfo.path = "/v5/trade";
    }
    
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
    connected_ = false;
}

// ============================================================================
// AUTHENTICATION & TRADING
// ============================================================================

std::string BybitWebSocketClient::generate_signature(long long expires) {
    std::string data = "GET/realtime" + std::to_string(expires);
    unsigned char* digest;
    
    digest = HMAC(EVP_sha256(), api_secret_.c_str(), api_secret_.length(), 
                  (unsigned char*)data.c_str(), data.length(), NULL, NULL);

    std::stringstream ss;
    for(int i = 0; i < 32; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    return ss.str();
}

void BybitWebSocketClient::authenticate() {
    if (channel_type_ != ChannelType::PRIVATE_TRADE) return;
    
    long long expires = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + 10000;

    std::string signature = generate_signature(expires);

    std::stringstream ss;
    ss << "{\"op\":\"auth\",\"args\":[\"" 
       << api_key_ << "\"," << expires << ",\"" << signature << "\"]}";
       
    std::string msg = ss.str();
    
    unsigned char buf[LWS_PRE + 1024];
    memcpy(&buf[LWS_PRE], msg.c_str(), msg.length());
    lws_write(wsi_, &buf[LWS_PRE], msg.length(), LWS_WRITE_TEXT);
    
    std::cout << "🔑 [Auth] Sending authentication request...\n";
}

void BybitWebSocketClient::place_order(const std::string& symbol, const std::string& side, 
                                      double qty, double price, const std::string& order_link_id) {
    
    if (!connected_ || channel_type_ != ChannelType::PRIVATE_TRADE) {
        std::cerr << "❌ Place Order Failed: Not connected or wrong channel.\n";
        return;
    }

    long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::stringstream ss;
    ss << std::fixed << std::setprecision(5);
    ss << "{"
       << "\"reqId\":\"" << order_link_id << "\","
       << "\"header\":{"
       <<    "\"X-BAPI-TIMESTAMP\":\"" << now << "\","
       <<    "\"X-BAPI-RECV-WINDOW\":\"5000\""
       << "},"
       << "\"op\":\"order.create\","
       << "\"args\":[{"
       <<    "\"symbol\":\"" << symbol << "\","
       <<    "\"side\":\"" << side << "\","
       <<    "\"orderType\":\"Limit\","
       <<    "\"qty\":\"" << qty << "\","
       <<    "\"price\":\"" << price << "\","
       <<    "\"category\":\"linear\","
       <<    "\"timeInForce\":\"PostOnly\","
       <<    "\"orderLinkId\":\"" << order_link_id << "\""
       << "}]}";

    std::string msg = ss.str();
    
    unsigned char buf[LWS_PRE + 2048];
    memcpy(&buf[LWS_PRE], msg.c_str(), msg.length());
    
    int n = lws_write(wsi_, &buf[LWS_PRE], msg.length(), LWS_WRITE_TEXT);
    if (n < 0) std::cerr << "❌ Failed to send Place Order\n";
    else std::cout << "📤 Order Sent: " << order_link_id << " (" << side << " " << qty << " @ " << price << ")\n";
}

void BybitWebSocketClient::cancel_order(const std::string& symbol, const std::string& order_link_id) {
    if (!connected_ || channel_type_ != ChannelType::PRIVATE_TRADE) return;

    long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::stringstream ss;
    ss << "{"
       << "\"header\":{"
       <<    "\"X-BAPI-TIMESTAMP\":\"" << now << "\","
       <<    "\"X-BAPI-RECV-WINDOW\":\"5000\""
       << "},"
       << "\"op\":\"order.cancel\","
       << "\"args\":[{"
       <<    "\"symbol\":\"" << symbol << "\","
       <<    "\"category\":\"linear\","
       <<    "\"orderLinkId\":\"" << order_link_id << "\""
       << "}]}";

    std::string msg = ss.str();
    
    unsigned char buf[LWS_PRE + 1024];
    memcpy(&buf[LWS_PRE], msg.c_str(), msg.length());
    lws_write(wsi_, &buf[LWS_PRE], msg.length(), LWS_WRITE_TEXT);
    
    std::cout << "📤 Cancel Sent: " << order_link_id << "\n";
}

// ============================================================================
// MARKET DATA (PUBLIC CHANNEL)
// ============================================================================

void BybitWebSocketClient::subscribe_to_symbol(const std::string& symbol) {
    if (!wsi_ || !connected_) {
        std::cerr << "❌ Cannot subscribe: WebSocket not connected yet\n";
        return;
    }
    
    orderbook_manager_.get_or_create(symbol);
    
    std::stringstream sub_msg;
    sub_msg << "{\"op\":\"subscribe\",\"args\":[\"orderbook.50." << symbol << "\"]}";
    
    std::string msg = sub_msg.str();
    unsigned char buf[LWS_PRE + 1024];
    memcpy(&buf[LWS_PRE], msg.c_str(), msg.length());
    
    int written = lws_write(wsi_, &buf[LWS_PRE], msg.length(), LWS_WRITE_TEXT);
    if (written > 0) {
        symbol_manager_.add_symbol(symbol);
        std::cout << "✅ Subscribed to " << symbol << "\n";
    }
}

// ============================================================================
// WEBSOCKET CALLBACKS
// ============================================================================

int BybitWebSocketClient::callback_function(
    struct lws* wsi,
    enum lws_callback_reasons reason,
    void* user,
    void* in,
    size_t len
) {
    BybitWebSocketClient* client = static_cast<BybitWebSocketClient*>(
        lws_context_user(lws_get_context(wsi)));
    auto* session = static_cast<SessionData*>(user);
    
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            std::cout << "✓ WebSocket connected (" 
                      << (client->channel_type_ == ChannelType::PUBLIC ? "Public" : "Private") 
                      << ")\n";
            if (session) session->rx_buffer.clear();
            client->connected_ = true; 
            break;
            
        case LWS_CALLBACK_CLIENT_RECEIVE: {
            if (!session || !client) break;
            session->rx_buffer.append(static_cast<char*>(in), len);
            
            if (lws_is_final_fragment(wsi)) {
                if (client->channel_type_ == ChannelType::PUBLIC) {
                    client->handle_message(const_cast<char*>(session->rx_buffer.data()), 
                                         session->rx_buffer.length());
                } else {
                    client->handle_order_update(const_cast<char*>(session->rx_buffer.data()), 
                                              session->rx_buffer.length());
                }
                session->rx_buffer.clear();
            }
            break;
        }
            
        case LWS_CALLBACK_CLIENT_CLOSED:
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            std::cout << "✗ WebSocket disconnected (" 
                      << (client ? (client->channel_type_ == ChannelType::PUBLIC ? "Public" : "Private") : "Unknown")
                      << ")\n";
            if (client) client->connected_ = false;
            if (session) session->rx_buffer.clear();
            break;
            
        default:
            break;
    }
    return 0;
}

// ============================================================================
// MESSAGE HANDLERS
// ============================================================================

void BybitWebSocketClient::handle_message(char* data, size_t len) {
    try {
        simdjson::padded_string padded(data, len);
        simdjson::ondemand::document doc = parser_.iterate(padded);
        
        auto success_result = doc["success"];
        if (!success_result.error() && success_result.get_bool().value()) {
            std::cout << "✅ Subscription confirmed\n";
            return;
        }
        
        auto topic_result = doc["topic"];
        if (topic_result.error()) return;
        
        std::string_view topic_str = topic_result.get_string().value();
        if (topic_str.find("orderbook") == std::string::npos) return;
        
        size_t last_dot = topic_str.rfind('.');
        if (last_dot == std::string::npos) return;
        std::string symbol(topic_str.substr(last_dot + 1));
        
        auto data_obj = doc["data"].get_object();
        auto orderbook = orderbook_manager_.get_or_create(symbol);
        
        std::vector<PriceLevel> bids, asks;
        
        auto bids_arr = data_obj["b"];
        if (!bids_arr.error()) {
            for (auto bid : bids_arr) {
                auto arr = bid.get_array();
                auto it = arr.begin();
                std::string price = std::string((*it).get_string().value());
                std::string qty = std::string((*(++it)).get_string().value());
                bids.push_back({std::stod(price), std::stod(qty)});
            }
        }
        
        auto asks_arr = data_obj["a"];
        if (!asks_arr.error()) {
            for (auto ask : asks_arr) {
                auto arr = ask.get_array();
                auto it = arr.begin();
                std::string price = std::string((*it).get_string().value());
                std::string qty = std::string((*(++it)).get_string().value());
                asks.push_back({std::stod(price), std::stod(qty)});
            }
        }
        
        if (!bids.empty()) orderbook->update_bids(bids);
        if (!asks.empty()) orderbook->update_asks(asks);
        if (!bids.empty() || !asks.empty()) orderbook->increment_update();
        
        if (config_.enable_aeron && aeron_pub_) {
            auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            sbe_encoder_.encode_orderbook_snapshot(
                timestamp, orderbook->get_bids(10), orderbook->get_asks(10), symbol);
            
            if (aeron_pub_->publish(sbe_encoder_.data(), sbe_encoder_.size())) {
                aeron_published_.fetch_add(1, std::memory_order_relaxed);
            }
        }
        
        messages_received_.fetch_add(1, std::memory_order_relaxed);
        
    } catch (const std::exception& e) {
    }
}

// [CRITICAL FIX AREA]
void BybitWebSocketClient::handle_order_update(char* data, size_t len) {
    try {
        simdjson::padded_string padded(data, len);
        simdjson::ondemand::document doc = parser_.iterate(padded);
        
        // 1. Check Operation Responses
        auto op_result = doc["op"];
        if (!op_result.error()) {
            std::string_view op = op_result.get_string().value();
            
            if (op == "auth") {
                auto ret_code = doc["retCode"].get_int64().value();
                if (ret_code == 0) {
                    std::cout << "🔐 Authentication SUCCESS\n";
                } else {
                    auto msg = doc["retMsg"].get_string().value();
                    std::cerr << "❌ Authentication FAILED: " << msg << "\n";
                }
                return;
            }

            // [FIXED] Order Create Ack
            if (op == "order.create") {
                auto ret_code = doc["retCode"].get_int64().value();
                
                if (ret_code == 0) {
                     // SUCCESS: Get the Client ID (orderLinkId)
                     // [FIX] We fetch 'orderLinkId' instead of 'orderId' to match TradingEngine
                     auto orderLinkId = doc["data"]["orderLinkId"].get_string().value();
                     std::cout << "✅ Order Accepted (Link ID: " << orderLinkId << ")\n";
                     
                     // [FIX] Uncommented and active
                     if (on_order_update_) {
                         on_order_update_(std::string(orderLinkId), "New", ""); 
                     }
                } else {
                     // FAILURE
                     auto msg = doc["retMsg"].get_string().value();
                     std::cerr << "❌ Order REJECTED: " << msg << "\n";
                     
                     // [FIX] Uncommented and active
                     // Use reqId as fallback to identify which order failed
                     auto reqId = doc["reqId"].get_string().value();
                     if (on_order_update_) {
                         on_order_update_(std::string(reqId), "Rejected", "");
                     }
                }
                return;
            }
            
            if (op == "order.cancel") {
                auto ret_code = doc["retCode"].get_int64().value();
                if (ret_code == 0) {
                     std::cout << "✅ Cancellation Accepted\n";
                } else {
                     auto msg = doc["retMsg"].get_string().value();
                     std::cerr << "❌ Cancel REJECTED: " << msg << "\n";
                }
                return;
            }
        }
        
        // 2. Handle Real-Time Execution Reports (Topic: execution)
        // This confirms fills/trades asynchronously
        auto topic_res = doc["topic"];
        if (!topic_res.error()) {
            std::string_view topic = topic_res.get_string().value();
            if (topic == "execution") {
                 auto data_arr = doc["data"].get_array();
                 for (auto item : data_arr) {
                     // Extract the BOT ID and the status
                     auto oid = item["orderLinkId"].get_string().value();
                     // Send "Filled" signal to engine
                     if (on_order_update_) {
                         on_order_update_(std::string(oid), "Filled", "");
                     }
                 }
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "⚠️  Trade Msg Error: " << e.what() << "\n";
    }
}

// ============================================================================
// METRICS
// ============================================================================

uint64_t BybitWebSocketClient::get_message_count() const {
    return messages_received_.load();
}

uint64_t BybitWebSocketClient::get_aeron_count() const {
    return aeron_published_.load();
}