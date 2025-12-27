#pragma once
#include <array>
#include <atomic>
#include <vector>

struct PriceLevel {
    double price;
    double quantity;
};

class OrderBook {
public:
    static constexpr int MAX_LEVELS = 10;
    
    void update_bids(const std::vector<PriceLevel>& bids);
    void update_asks(const std::vector<PriceLevel>& asks);
    
    bool get_best_bid(double& price, double& qty) const;
    bool get_best_ask(double& price, double& qty) const;
    double get_fair_price() const;
    
    std::vector<std::pair<double, double>> get_bids(int max_levels = 10) const;
    std::vector<std::pair<double, double>> get_asks(int max_levels = 10) const;
    
    void increment_update();
    uint64_t get_update_count() const;

private:
    std::array<PriceLevel, MAX_LEVELS> bids_;
    std::array<PriceLevel, MAX_LEVELS> asks_;
    std::atomic<int> bid_count_{0};
    std::atomic<int> ask_count_{0};
    std::atomic<uint64_t> update_id_{0};
};