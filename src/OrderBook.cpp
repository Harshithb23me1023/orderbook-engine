#include "OrderBook.hpp"
#include <iomanip>
#include <algorithm>

OrderBook::OrderBook(std::string symbol) : symbol_(std::move(symbol)) {}

double OrderBook::getBestBid() const {
    if (bids_.empty()) return 0.0;
    return bids_.begin()->first;
}

double OrderBook::getBestAsk() const {
    if (asks_.empty()) return 0.0;
    return asks_.begin()->first;
}

double OrderBook::getSpread() const {
    double bestBid = getBestBid();
    double bestAsk = getBestAsk();
    if (bestBid <= 0.0 || bestAsk <= 0.0) return 0.0;
    return bestAsk - bestBid;
}

double OrderBook::getMidPrice() const {
    double bestBid = getBestBid();
    double bestAsk = getBestAsk();
    if (bestBid <= 0.0 || bestAsk <= 0.0) return 0.0;
    return (bestBid + bestAsk) / 2.0;
}

double OrderBook::getOrderFlowImbalance(size_t depthLevels) const {
    uint32_t totalBidQty = 0;
    uint32_t totalAskQty = 0;

    size_t count = 0;
    for (const auto& pair : bids_) {
        if (count++ >= depthLevels) break;
        for (const auto& ord : pair.second) totalBidQty += ord.quantity;
    }

    count = 0;
    for (const auto& pair : asks_) {
        if (count++ >= depthLevels) break;
        for (const auto& ord : pair.second) totalAskQty += ord.quantity;
    }

    if (totalBidQty + totalAskQty == 0) return 0.0;
    return static_cast<double>(static_cast<int64_t>(totalBidQty) - static_cast<int64_t>(totalAskQty)) /
           (totalBidQty + totalAskQty);
}

size_t OrderBook::totalOrdersCount() const {
    return orderIndex_.size();
}

bool OrderBook::cancelOrder(uint64_t orderId) {
    auto it = orderIndex_.find(orderId);
    if (it == orderIndex_.end()) {
        return false;
    }

    const OrderLocation& loc = it->second;
    if (loc.side == Side::BUY) {
        auto levelIt = bids_.find(loc.price);
        if (levelIt != bids_.end()) {
            levelIt->second.erase(loc.it);
            if (levelIt->second.empty()) {
                bids_.erase(levelIt);
            }
        }
    } else {
        auto levelIt = asks_.find(loc.price);
        if (levelIt != asks_.end()) {
            levelIt->second.erase(loc.it);
            if (levelIt->second.empty()) {
                asks_.erase(levelIt);
            }
        }
    }

    orderIndex_.erase(it);
    return true;
}

std::vector<Trade> OrderBook::addOrder(const Order& order) {
    if (order.side == Side::BUY) {
        if (order.type == OrderType::MARKET) {
            return matchMarketBuy(order);
        }
        return matchLimitBuy(order);
    } else {
        if (order.type == OrderType::MARKET) {
            return matchMarketSell(order);
        }
        return matchLimitSell(order);
    }
}

std::vector<Trade> OrderBook::matchLimitBuy(Order order) {
    std::vector<Trade> trades;

    // Match against asks while order has remaining quantity and ask price <= buy limit price
    while (order.quantity > 0 && !asks_.empty()) {
        auto bestAskIt = asks_.begin();
        double bestAskPrice = bestAskIt->first;

        if (bestAskPrice > order.price) {
            break; // Highest ask exceeds limit price
        }

        auto& askOrders = bestAskIt->second;
        while (order.quantity > 0 && !askOrders.empty()) {
            Order& standingOrder = askOrders.front();
            uint32_t matchQty = std::min(order.quantity, standingOrder.quantity);

            Trade t;
            t.tradeId = ++tradeIdCounter_;
            t.buyerOrderId = order.id;
            t.sellerOrderId = standingOrder.id;
            t.symbol = symbol_;
            t.price = bestAskPrice; // Standing order sets price
            t.quantity = matchQty;
            t.timestamp = order.timestamp;
            trades.push_back(t);

            order.quantity -= matchQty;
            standingOrder.quantity -= matchQty;

            if (standingOrder.quantity == 0) {
                orderIndex_.erase(standingOrder.id);
                askOrders.pop_front();
            }
        }

        if (askOrders.empty()) {
            asks_.erase(bestAskIt);
        }
    }

    // If resting quantity remains and not IOC, add to bids book
    if (order.quantity > 0 && order.type != OrderType::IOC) {
        auto& list = bids_[order.price];
        list.push_back(order);
        auto orderIt = std::prev(list.end());
        orderIndex_[order.id] = {Side::BUY, order.price, orderIt};
    }

    return trades;
}

std::vector<Trade> OrderBook::matchLimitSell(Order order) {
    std::vector<Trade> trades;

    // Match against bids while order has remaining quantity and bid price >= sell limit price
    while (order.quantity > 0 && !bids_.empty()) {
        auto bestBidIt = bids_.begin();
        double bestBidPrice = bestBidIt->first;

        if (bestBidPrice < order.price) {
            break; // Highest bid is lower than limit price
        }

        auto& bidOrders = bestBidIt->second;
        while (order.quantity > 0 && !bidOrders.empty()) {
            Order& standingOrder = bidOrders.front();
            uint32_t matchQty = std::min(order.quantity, standingOrder.quantity);

            Trade t;
            t.tradeId = ++tradeIdCounter_;
            t.buyerOrderId = standingOrder.id;
            t.sellerOrderId = order.id;
            t.symbol = symbol_;
            t.price = bestBidPrice; // Standing order sets price
            t.quantity = matchQty;
            t.timestamp = order.timestamp;
            trades.push_back(t);

            order.quantity -= matchQty;
            standingOrder.quantity -= matchQty;

            if (standingOrder.quantity == 0) {
                orderIndex_.erase(standingOrder.id);
                bidOrders.pop_front();
            }
        }

        if (bidOrders.empty()) {
            bids_.erase(bestBidIt);
        }
    }

    if (order.quantity > 0 && order.type != OrderType::IOC) {
        auto& list = asks_[order.price];
        list.push_back(order);
        auto orderIt = std::prev(list.end());
        orderIndex_[order.id] = {Side::SELL, order.price, orderIt};
    }

    return trades;
}

std::vector<Trade> OrderBook::matchMarketBuy(Order order) {
    std::vector<Trade> trades;
    while (order.quantity > 0 && !asks_.empty()) {
        auto bestAskIt = asks_.begin();
        double bestAskPrice = bestAskIt->first;
        auto& askOrders = bestAskIt->second;

        while (order.quantity > 0 && !askOrders.empty()) {
            Order& standingOrder = askOrders.front();
            uint32_t matchQty = std::min(order.quantity, standingOrder.quantity);

            Trade t{++tradeIdCounter_, order.id, standingOrder.id, symbol_, bestAskPrice, matchQty, order.timestamp};
            trades.push_back(t);

            order.quantity -= matchQty;
            standingOrder.quantity -= matchQty;

            if (standingOrder.quantity == 0) {
                orderIndex_.erase(standingOrder.id);
                askOrders.pop_front();
            }
        }
        if (askOrders.empty()) asks_.erase(bestAskIt);
    }
    return trades;
}

std::vector<Trade> OrderBook::matchMarketSell(Order order) {
    std::vector<Trade> trades;
    while (order.quantity > 0 && !bids_.empty()) {
        auto bestBidIt = bids_.begin();
        double bestBidPrice = bestBidIt->first;
        auto& bidOrders = bestBidIt->second;

        while (order.quantity > 0 && !bidOrders.empty()) {
            Order& standingOrder = bidOrders.front();
            uint32_t matchQty = std::min(order.quantity, standingOrder.quantity);

            Trade t{++tradeIdCounter_, standingOrder.id, order.id, symbol_, bestBidPrice, matchQty, order.timestamp};
            trades.push_back(t);

            order.quantity -= matchQty;
            standingOrder.quantity -= matchQty;

            if (standingOrder.quantity == 0) {
                orderIndex_.erase(standingOrder.id);
                bidOrders.pop_front();
            }
        }
        if (bidOrders.empty()) bids_.erase(bestBidIt);
    }
    return trades;
}

void OrderBook::printMarketDepth(size_t levels) const {
    std::cout << "\n============================================\n";
    std::cout << "        MARKET DEPTH: " << symbol_ << "        \n";
    std::cout << "============================================\n";
    std::cout << std::setw(10) << "BID QTY" << " | "
              << std::setw(10) << "BID PRICE" << " || "
              << std::setw(10) << "ASK PRICE" << " | "
              << std::setw(10) << "ASK QTY" << "\n";
    std::cout << "--------------------------------------------\n";

    auto bidIt = bids_.begin();
    auto askIt = asks_.begin();

    for (size_t i = 0; i < levels; ++i) {
        std::string bidQtyStr = "-", bidPxStr = "-";
        std::string askPxStr = "-", askQtyStr = "-";

        if (bidIt != bids_.end()) {
            uint32_t qty = 0;
            for (const auto& ord : bidIt->second) qty += ord.quantity;
            bidQtyStr = std::to_string(qty);
            bidPxStr = std::to_string(bidIt->first).substr(0, 7);
            ++bidIt;
        }

        if (askIt != asks_.end()) {
            uint32_t qty = 0;
            for (const auto& ord : askIt->second) qty += ord.quantity;
            askQtyStr = std::to_string(qty);
            askPxStr = std::to_string(askIt->first).substr(0, 7);
            ++askIt;
        }

        if (bidPxStr == "-" && askPxStr == "-") break;

        std::cout << std::setw(10) << bidQtyStr << " | "
                  << std::setw(10) << bidPxStr << " || "
                  << std::setw(10) << askPxStr << " | "
                  << std::setw(10) << askQtyStr << "\n";
    }

    std::cout << "--------------------------------------------\n";
    std::cout << "Best Bid: " << getBestBid() << " | Best Ask: " << getBestAsk() 
              << " | Spread: " << getSpread() << " | OFI: " << getOrderFlowImbalance() << "\n";
    std::cout << "============================================\n\n";
}
