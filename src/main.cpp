#include "OrderBook.hpp"
#include <iostream>
#include <random>
#include <chrono>

int main() {
    std::cout << "=========================================================\n";
    std::cout << "   NEXUS MATCHING CORE - High-Throughput LOB Engine     \n";
    std::cout << "=========================================================\n";

    OrderBook book("NIFTY_FUT");

    std::cout << "[1] Injecting initial liquidity to build order book depth...\n";

    // Inject bids
    book.addOrder(Order(1, "NIFTY_FUT", Side::BUY, OrderType::LIMIT, 22450.0, 50));
    book.addOrder(Order(2, "NIFTY_FUT", Side::BUY, OrderType::LIMIT, 22449.5, 100));
    book.addOrder(Order(3, "NIFTY_FUT", Side::BUY, OrderType::LIMIT, 22449.0, 150));
    book.addOrder(Order(4, "NIFTY_FUT", Side::BUY, OrderType::LIMIT, 22448.5, 75));

    // Inject asks
    book.addOrder(Order(5, "NIFTY_FUT", Side::SELL, OrderType::LIMIT, 22451.0, 40));
    book.addOrder(Order(6, "NIFTY_FUT", Side::SELL, OrderType::LIMIT, 22451.5, 80));
    book.addOrder(Order(7, "NIFTY_FUT", Side::SELL, OrderType::LIMIT, 22452.0, 120));
    book.addOrder(Order(8, "NIFTY_FUT", Side::SELL, OrderType::LIMIT, 22452.5, 60));

    book.printMarketDepth();

    std::cout << "[2] Submitting aggressive Market Order to cross the spread...\n";
    // Aggressive buy market order for 60 units (takes all 40 @ 22451.0 and 20 @ 22451.5)
    auto trades = book.addOrder(Order(9, "NIFTY_FUT", Side::BUY, OrderType::MARKET, 0.0, 60));
    std::cout << "Executed " << trades.size() << " trades:\n";
    for (const auto& t : trades) {
        std::cout << " -> Trade #" << t.tradeId << ": Matched " << t.quantity 
                  << " @ " << t.price << " (Buyer: " << t.buyerOrderId 
                  << ", Seller: " << t.sellerOrderId << ")\n";
    }

    book.printMarketDepth();

    std::cout << "[3] Testing O(1) Cancellation of resting Order #3...\n";
    bool cancelled = book.cancelOrder(3);
    std::cout << "Order #3 Cancellation Status: " << (cancelled ? "SUCCESS" : "FAILED") << "\n";

    std::cout << "\n[4] Running Synthetic High-Throughput Benchmark (100,000 orders)...\n";
    const int NUM_ORDERS = 100000;
    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> priceDist(22440.0, 22460.0);
    std::uniform_int_distribution<uint32_t> qtyDist(1, 100);
    std::uniform_int_distribution<int> sideDist(0, 1);

    auto start = std::chrono::high_resolution_clock::now();
    uint64_t totalTrades = 0;

    for (int i = 100; i < 100 + NUM_ORDERS; ++i) {
        Side s = (sideDist(rng) == 0) ? Side::BUY : Side::SELL;
        double px = std::round(priceDist(rng) * 2.0) / 2.0; // 0.5 tick size
        uint32_t qty = qtyDist(rng);
        auto t = book.addOrder(Order(i, "NIFTY_FUT", s, OrderType::LIMIT, px, qty));
        totalTrades += t.size();
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> durationMs = end - start;

    double ordersPerSec = (NUM_ORDERS / (durationMs.count() / 1000.0));
    std::cout << "Benchmark Completed:\n";
    std::cout << " -> Processed: " << NUM_ORDERS << " orders in " << durationMs.count() << " ms\n";
    std::cout << " -> Throughput: " << static_cast<uint64_t>(ordersPerSec) << " orders/sec\n";
    std::cout << " -> Total Trades Generated: " << totalTrades << "\n";
    std::cout << " -> Resting Orders in Book: " << book.totalOrdersCount() << "\n";

    std::cout << "\nNexus Matching Core Execution Finished.\n";
    return 0;
}
