#pragma once
#include "network/BybitWebSocketClient.h"
#include "core/OrderBookManager.h"
#include "utils/DataLogger.h"
#include <atomic>

class PerformanceMonitor {
public:
    PerformanceMonitor(
        BybitWebSocketClient& ws,
        OrderBookManager& obm,
        DataLogger& logger
    );
    
    void run();
    void stop();

private:
    BybitWebSocketClient& ws_client_;
    OrderBookManager& orderbook_manager_;
    DataLogger& data_logger_;
    std::atomic<bool> running_{true};
};