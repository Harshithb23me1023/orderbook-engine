#pragma once
#include <cstdint>
#include <string>
#include <chrono>

enum class Side {
    BUY,
    SELL
};

enum class OrderType {
    LIMIT,
    MARKET,
    IOC // Immediate Or Cancel
};

struct Order {
    uint64_t id;
    std::string symbol;
    Side side;
    OrderType type;
    double price;
    uint32_t quantity;
    uint32_t initialQuantity;
    uint64_t timestamp;

    Order(uint64_t id_, const std::string& symbol_, Side side_, OrderType type_, double price_, uint32_t qty_)
        : id(id_), symbol(symbol_), side(side_), type(type_), price(price_),
          quantity(qty_), initialQuantity(qty_),
          timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::high_resolution_clock::now().time_since_epoch()).count()) {}
};

struct Trade {
    uint64_t tradeId;
    uint64_t buyerOrderId;
    uint64_t sellerOrderId;
    std::string symbol;
    double price;
    uint32_t quantity;
    uint64_t timestamp;
};
