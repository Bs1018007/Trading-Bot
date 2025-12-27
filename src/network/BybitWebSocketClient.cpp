#include "network/BybitWebSocketClient.h"
#include <iostream>
#include <cstring>
#include <chrono>

// [FIX] Define SessionData struct to buffer fragmented messages
struct SessionData {
    std::string rx_buffer;
};

// [FIX] Update protocols to allocate memory for SessionData
struct lws_protocols BybitWebSocketClient::protocols_[] = {
    { "bybit-protocol", BybitWebSocketClient::callback_function, sizeof(SessionData), 65536 },
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
    // Note: prepare_subscription_messages() removed - not needed for single symbol subscription
    // If you need to subscribe to many symbols, uncomment and use subscribe_orderbooks()
    
    if (config_.enable_aeron) {
        aeron_pub_ = std::make_unique<AeronPublisher>(
            config_.aeron_channel, config_.orderbook_stream_id);
        // ** Initialize Aeron Publisher **
        if (!aeron_pub_->init()) {
            std::cerr << "⚠ Aeron disabled - continuing without IPC\n";
            // We keep the object alive so internal buffer logic still works
             // aeron_pub_.reset(); // Commented out to allow local buffering
        }
    }
}

BybitWebSocketClient::~BybitWebSocketClient() {
    running_ = false;
    if (context_) {
        lws_context_destroy(context_);
    }
}

// Removed prepare_subscription_messages() - not needed for single symbol subscription
// If you need to subscribe to many symbols at once, you can restore this function
// and use subscribe_orderbooks() instead of subscribe_to_symbol()

// ============================================================================
// [FIX] REWRITTEN CALLBACK TO HANDLE FRAGMENTATION
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
    
    // [FIX] Cast user pointer to SessionData
    auto* session = static_cast<SessionData*>(user);
    
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            std::cout << "✓ WebSocket connected\n";
            // [FIX] Clear buffer on new connection
            if (session) session->rx_buffer.clear();
            // Don't auto-subscribe - let main.cpp handle subscriptions
            // client->subscribe_orderbooks();
            break;
            
        case LWS_CALLBACK_CLIENT_RECEIVE: {
            // [FIX] Accumulate data instead of processing immediately
            if (!session || !client) break;

            // 1. Append incoming chunk to buffer
            session->rx_buffer.append(static_cast<char*>(in), len);
            
            // 2. Only process if this is the final fragment
            if (lws_is_final_fragment(wsi)) {
                
                // [Optional Debugging for Snapshots]
                if (session->rx_buffer.length() > 2000) {
                     std::cout << "📩 Received large message: " << session->rx_buffer.length() << " bytes\n";
                 } else {
                    static int msg_count = 0;
                    if (msg_count++ < 3) {
                        std::cout << "\n📩 Received message #" << msg_count << " (" << session->rx_buffer.length() << " bytes)\n";
                    }
                }
                
                // 3. Process the full message
                // Note: We use data() and length() from the std::string buffer
                // The const_cast is safe here because handle_message expects char* but treats it as read-only for parsing
                client->handle_message(const_cast<char*>(session->rx_buffer.data()), session->rx_buffer.length());
                
                // 4. Clear buffer for next message
                session->rx_buffer.clear();
            }
            break;
        }
            
        case LWS_CALLBACK_CLIENT_CLOSED:
            std::cout << "✗ WebSocket closed\n";
            if (session) session->rx_buffer.clear();
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
    std::cout << "\n🚀 Sending " << subscription_messages_.size() 
             << " subscription batches...\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < subscription_messages_.size(); ++i) {
        const std::string& sub_msg = subscription_messages_[i];
        
        // Allocate buffer with LWS_PRE padding
        unsigned char buf[LWS_PRE + 8192];
        
        // Copy message after padding
        memcpy(&buf[LWS_PRE], sub_msg.c_str(), sub_msg.length());
        
        // Send to WebSocket
        int written = lws_write(wsi_, &buf[LWS_PRE], sub_msg.length(), LWS_WRITE_TEXT);
        
        if (written < 0) {
            std::cerr << "✗ Failed to send batch " << (i + 1) << "\n";
        } else {
            std::cout << "  ✓ Sent batch " << (i + 1) << "/" << subscription_messages_.size() 
                     << " (" << written << " bytes)\n";
        }
        
        // Small delay between batches to avoid overwhelming the server
        if (i < subscription_messages_.size() - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    std::cout << "\n✅ All subscriptions sent in " << duration << " μs\n";
    std::cout << "⚡ Average per batch: " << (duration / subscription_messages_.size()) << " μs\n\n";
}

/*void BybitWebSocketClient::subscribe_orderbooks() {
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
        symbol_manager_.add_symbol(sym);
    }
}*/

void BybitWebSocketClient::handle_message(char* data, size_t len) {
    try {
        simdjson::padded_string padded(data, len);
        simdjson::ondemand::document doc = parser_.iterate(padded);
        
        // Check if this is a subscription response message
        auto success_result = doc["success"];
        if (!success_result.error()) {
            bool success = success_result.get_bool().value();
            if (success) {
                std::cout << "✅ Subscription confirmed by Bybit\n";
            } else {
                // Log subscription error (but don't spam for order operation errors)
                auto ret_msg_result = doc["ret_msg"];
                if (!ret_msg_result.error()) {
                    std::string_view ret_msg = ret_msg_result.get_string().value();
                    // Only log if it's not an order operation error (expected on public channel)
                    if (ret_msg.find("order") == std::string::npos && 
                        ret_msg.find("CmdReq") == std::string::npos) {
                        std::cerr << "❌ Subscription failed: " << std::string(ret_msg) << "\n";
                    }
                    // Order operations on public channel will fail - this is expected
                }
            }
            return;
        }
        
        // Check if this is an orderbook update
        auto topic_result = doc["topic"];
        if (topic_result.error()) {
            // Might be an order update or other message type
            return;
        }
        
        std::string_view topic_str = topic_result.get_string().value();
        
        // Check if it's an orderbook topic
        if (topic_str.find("orderbook") == std::string::npos) {
            // Not an orderbook message
            return;
        }
        
        // Extract symbol from topic (format: "orderbook.50.BTCUSDT")
        size_t last_dot = topic_str.rfind('.');
        if (last_dot == std::string::npos) return;
        
        std::string symbol(topic_str.substr(last_dot + 1));
        
        // Get data object
        auto data_obj_result = doc["data"];
        if (data_obj_result.error()) {
            std::cerr << "⚠️  Orderbook message missing 'data' field\n";
            return;
        }
        
        auto orderbook = orderbook_manager_.get_or_create(symbol);
        auto data_obj = data_obj_result.get_object();
        
        // Parse bids
        std::vector<PriceLevel> bids;
        auto bids_result = data_obj["b"];
        if (!bids_result.error()) {
            auto bids_array_result = bids_result.get_array();
            if (!bids_array_result.error()) {
                auto bids_array = bids_array_result;
                for (auto bid : bids_array) {
                    auto bid_array_result = bid.get_array();
                    if (bid_array_result.error()) continue;
                    
                    auto bid_array = bid_array_result;
                    auto it = bid_array.begin();
                    if (it == bid_array.end()) continue;
                    
                    auto price_result = (*it).get_string();
                    if (price_result.error()) continue;
                    std::string_view price_sv = price_result.value();
                    
                    ++it;
                    if (it == bid_array.end()) continue;
                    
                    auto qty_result = (*it).get_string();
                    if (qty_result.error()) continue;
                    std::string_view qty_sv = qty_result.value();
                    
                    try {
                        bids.push_back({
                            std::stod(std::string(price_sv)),
                            std::stod(std::string(qty_sv))
                        });
                    } catch (...) {
                        // Skip invalid price/qty
                    }
                }
            }
        }
        
        // Parse asks
        std::vector<PriceLevel> asks;
        auto asks_result = data_obj["a"];
        if (!asks_result.error()) {
            auto asks_array_result = asks_result.get_array();
            if (!asks_array_result.error()) {
                auto asks_array = asks_array_result;
                for (auto ask : asks_array) {
                    auto ask_array_result = ask.get_array();
                    if (ask_array_result.error()) continue;
                    
                    auto ask_array = ask_array_result;
                    auto it = ask_array.begin();
                    if (it == ask_array.end()) continue;
                    
                    auto price_result = (*it).get_string();
                    if (price_result.error()) continue;
                    std::string_view price_sv = price_result.value();
                    
                    ++it;
                    if (it == ask_array.end()) continue;
                    
                    auto qty_result = (*it).get_string();
                    if (qty_result.error()) continue;
                    std::string_view qty_sv = qty_result.value();
                    
                    try {
                        asks.push_back({
                            std::stod(std::string(price_sv)),
                            std::stod(std::string(qty_sv))
                        });
                    } catch (...) {
                        // Skip invalid price/qty
                    }
                }
            }
        }
        
        // Only update if we have at least one side of data
        if (bids.empty() && asks.empty()) {
            // Skip updates with no data
            return;
        }
        
        // Update orderbook with new data (only update sides that have data)
        // This prevents overwriting existing data with empty vectors
        if (!bids.empty()) {
            orderbook->update_bids(bids);
        }
        if (!asks.empty()) {
            orderbook->update_asks(asks);
        }
        
        // Only increment update counter if we actually updated something
        if (!bids.empty() || !asks.empty()) {
            orderbook->increment_update();
        }
        
        // Log first few updates for debugging
        uint64_t update_count = orderbook->get_update_count();
        if (update_count <= 3 || update_count % 100 == 0) {
            std::cout << "📊 Orderbook update #" << update_count 
                     << " for " << symbol;
            if (!bids.empty() && !asks.empty()) {
                std::cout << " | Bid: " << bids[0].price 
                         << " | Ask: " << asks[0].price;
            } else {
                std::cout << " | Bids: " << bids.size() << " | Asks: " << asks.size();
            }
            std::cout << "\n";
        }
        
        // Log orderbook data
        auto bids_log = orderbook->get_bids(5);
        auto asks_log = orderbook->get_asks(5);
        double mid_price = 0.0;
        if (!bids_log.empty() && !asks_log.empty()) {
            mid_price = (bids_log[0].first + asks_log[0].first) / 2.0;
        }
        data_logger_.log_orderbook(symbol, mid_price, bids_log, asks_log);
        
        if (config_.enable_aeron && aeron_pub_) {
            auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            auto bids_vec = orderbook->get_bids(10);
            auto asks_vec = orderbook->get_asks(10);
            
            sbe_encoder_.encode_orderbook_snapshot(
                timestamp, bids_vec, asks_vec, symbol);
            
            // Publish (returns false if no subscriber, but logic handles it)
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
        
    } catch (const simdjson::simdjson_error& e) {
        // Log parse errors for debugging
        static int error_count = 0;
        if (error_count++ < 5) {
            std::cerr << "⚠️  JSON parse error: " << e.what() << "\n";
        }
    } catch (const std::exception& e) {
        static int error_count = 0;
        if (error_count++ < 5) {
            std::cerr << "⚠️  Message handling error: " << e.what() << "\n";
        }
    }
}
void BybitWebSocketClient::handle_order_update(char* data, size_t len) {
    try {
        simdjson::padded_string padded(data, len);
        simdjson::ondemand::document doc = parser_.iterate(padded);
        
        auto data_obj = doc["data"];
        if (data_obj.error()) return;
        
        // Parse order details
        std::string order_id, status, symbol;
        
        auto order_id_result = data_obj["orderLinkId"];
        if (!order_id_result.error()) {
            order_id = std::string(order_id_result.get_string().value());
        }
        
        auto status_result = data_obj["orderStatus"];
        if (!status_result.error()) {
            status = std::string(status_result.get_string().value());
        }
        
        auto symbol_result = data_obj["symbol"];
        if (!symbol_result.error()) {
            symbol = std::string(symbol_result.get_string().value());
        }
        
        std::cout << "📬 Order Update: " << order_id 
                  << " | Status: " << status 
                  << " | Symbol: " << symbol << "\n";
        
        // Call callback if registered
        if (order_update_callback_) {
            order_update_callback_(order_id, status, symbol);
        }
        
    } catch (const simdjson::simdjson_error& e) {
        std::cerr << "❌ Failed to parse order update: " << e.what() << "\n";
    }
}
void BybitWebSocketClient::subscribe_to_symbol(const std::string& symbol) {
    if (symbol_manager_.is_subscribed(symbol)) {
        std::cout << "ℹ️  Already subscribed to: " << symbol << "\n";
        return;
    }
    
    // Check if WebSocket is connected
    if (!wsi_) {
        std::cerr << "❌ Cannot subscribe: WebSocket not connected yet\n";
        return;
    }
    
    // Pre-create orderbook so it exists immediately
    orderbook_manager_.get_or_create(symbol);
    
    // Subscribe to ORDERBOOK only (order updates require private channel with auth)
    std::stringstream sub_msg;
    sub_msg << "{"
            << "\"op\":\"subscribe\","
            << "\"args\":["
            << "\"orderbook.50." << symbol << "\""    // Orderbook only (public channel)
            << "]}";
    
    std::string msg = sub_msg.str();
    
    unsigned char buf[LWS_PRE + 1024];
    memcpy(&buf[LWS_PRE], msg.c_str(), msg.length());
    
    int written = lws_write(wsi_, &buf[LWS_PRE], msg.length(), LWS_WRITE_TEXT);
    
    if (written > 0) {
        symbol_manager_.add_symbol(symbol);
        std::cout << "✅ Subscribed to orderbook for: " << symbol << "\n";
        std::cout << "   📊 Orderbook pre-created, waiting for first update...\n";
        std::cout << "   📤 Sent " << written << " bytes subscription message\n";
        std::cout << "   ⚠️  Note: Order updates require private channel (not implemented)\n";
    } else {
        std::cerr << "❌ Failed to send subscription message for: " << symbol << "\n";
        std::cerr << "   Error: lws_write returned " << written << "\n";
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