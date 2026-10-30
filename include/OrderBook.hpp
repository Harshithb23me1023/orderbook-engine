#pragma once
#include "Order.hpp"
#include <map>
#include <list>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <memory>

class OrderBook {
public:
    using OrderList = std::list<Order>;
    using OrderIterator = OrderList::iterator;

    struct OrderLocation {
        Side side;
        double price;
        OrderIterator it;
    };

    explicit OrderBook(std::string symbol);

    // Core Matching Operations
    std::vector<Trade> addOrder(const Order& order);
    bool cancelOrder(uint64_t orderId);

    // Market Depth & Microstructure Metrics
    double getBestBid() const;
    double getBestAsk() const;
    double getSpread() const;
    double getMidPrice() const;
    double getOrderFlowImbalance(size_t depthLevels = 5) const;

    void printMarketDepth(size_t levels = 5) const;
    size_t totalOrdersCount() const;

private:
    std::string symbol_;
    uint64_t tradeIdCounter_{0};

    // Bids sorted DESCENDING: highest price priority
    std::map<double, OrderList, std::greater<double>> bids_;

    // Asks sorted ASCENDING: lowest price priority
    std::map<double, OrderList, std::less<double>> asks_;

    // O(1) Fast Lookup for cancellations
    std::unordered_map<uint64_t, OrderLocation> orderIndex_;

    std::vector<Trade> matchLimitBuy(Order order);
    std::vector<Trade> matchLimitSell(Order order);
    std::vector<Trade> matchMarketBuy(Order order);
    std::vector<Trade> matchMarketSell(Order order);
};
