#include "include/orderbook/OrderBook.h"
#include <iostream>
#include <iomanip>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <sstream>

// Typedef to avoid ambiguity
typedef OrderBook::OrderBook OrderBookImpl;

using namespace OrderBook;

// Helper function to print trades
void printTrade(const Trade& trade) {
    std::cout << "TRADE: BuyOrderId=" << trade.buyOrderId 
              << ", SellOrderId=" << trade.sellOrderId 
              << ", Symbol=" << trade.symbol 
              << ", Quantity=" << trade.quantity 
              << ", Price=" << trade.price << std::endl;
}

// Helper function to print order book state
void printOrderBook(const OrderBookImpl& book) {
    std::cout << "\n===== ORDER BOOK: " << book.getSymbol() << " =====\n";
    
    // Print best bid/ask
    std::cout << "Best bid: " << book.getBestBid() << std::endl;
    std::cout << "Best ask: " << book.getBestAsk() << std::endl;
    
    std::cout << "==============================\n";
}

// Helper function to generate random orders
OrderPtr generateRandomOrder(OrderId& nextOrderId, const std::string& symbol) {
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<int> side_dist(0, 1);
    static std::uniform_int_distribution<int> type_dist(0, 3);
    static std::uniform_int_distribution<int> price_dist(95, 105);
    static std::uniform_int_distribution<int> qty_dist(1, 10);
    
    Side side = static_cast<Side>(side_dist(rng));
    OrderType type = static_cast<OrderType>(type_dist(rng));
    Quantity qty = qty_dist(rng);
    Price price = price_dist(rng);
    Price stopPrice = price_dist(rng);
    
    OrderPtr order;
    switch(type) {
        case OrderType::MARKET:
            order = std::make_shared<MarketOrder>(nextOrderId++, symbol, side, qty);
            break;
        case OrderType::LIMIT:
            order = std::make_shared<LimitOrder>(nextOrderId++, symbol, side, qty, price);
            break;
        case OrderType::STOP:
            order = std::make_shared<StopOrder>(nextOrderId++, symbol, side, qty, stopPrice);
            break;
        case OrderType::STOP_LIMIT:
            order = std::make_shared<StopLimitOrder>(nextOrderId++, symbol, side, qty, price, stopPrice);
            break;
    }
    
    return order;
}

// Benchmark the order book performance
void benchmarkOrderBook(int numOrders) {
    std::cout << "Starting benchmark with " << numOrders << " orders..." << std::endl;
    
    OrderBookImpl book("AAPL");
    OrderId nextOrderId = 1;
    
    // Initialize with some limit orders to provide liquidity
    for (int i = 90; i < 100; ++i) {
        book.processOrder(std::make_shared<LimitOrder>(nextOrderId++, "AAPL", Side::BUY, 10, i));
    }
    
    for (int i = 101; i < 110; ++i) {
        book.processOrder(std::make_shared<LimitOrder>(nextOrderId++, "AAPL", Side::SELL, 10, i));
    }
    
    // Benchmark processing random orders
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numOrders; ++i) {
        OrderPtr order = generateRandomOrder(nextOrderId, "AAPL");
        book.processOrder(order);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    
    std::chrono::duration<double> elapsed = end - start;
    double ordersPerSecond = numOrders / elapsed.count();
    
    std::cout << "Processed " << numOrders << " orders in " 
              << elapsed.count() << " seconds" << std::endl;
    std::cout << "Throughput: " << std::fixed << std::setprecision(0) 
              << ordersPerSecond << " orders/second" << std::endl;
}

// Example usage of the order book
void orderBookExample() {
    OrderBookImpl book("AAPL");
    OrderId nextOrderId = 1;
    std::vector<Trade> trades;
    
    std::cout << "=== Order Book Example ===" << std::endl;
    
    // Add some limit orders
    std::cout << "\nAdding limit orders..." << std::endl;
    
    // Buy orders
    trades = book.processOrder(std::make_shared<LimitOrder>(nextOrderId++, "AAPL", Side::BUY, 10, 98));
    trades = book.processOrder(std::make_shared<LimitOrder>(nextOrderId++, "AAPL", Side::BUY, 5, 99));
    trades = book.processOrder(std::make_shared<LimitOrder>(nextOrderId++, "AAPL", Side::BUY, 7, 97));
    
    // Sell orders
    trades = book.processOrder(std::make_shared<LimitOrder>(nextOrderId++, "AAPL", Side::SELL, 3, 101));
    trades = book.processOrder(std::make_shared<LimitOrder>(nextOrderId++, "AAPL", Side::SELL, 8, 102));
    trades = book.processOrder(std::make_shared<LimitOrder>(nextOrderId++, "AAPL", Side::SELL, 5, 100));
    
    printOrderBook(book);
    
    // Add a market buy order
    std::cout << "\nAdding market buy order..." << std::endl;
    trades = book.processOrder(std::make_shared<MarketOrder>(nextOrderId++, "AAPL", Side::BUY, 4));
    
    for (const auto& trade : trades) {
        printTrade(trade);
    }
    
    printOrderBook(book);
    
    // Add a limit order that crosses the book
    std::cout << "\nAdding limit buy order that crosses the book..." << std::endl;
    trades = book.processOrder(std::make_shared<LimitOrder>(nextOrderId++, "AAPL", Side::BUY, 6, 102));
    
    for (const auto& trade : trades) {
        printTrade(trade);
    }
    
    printOrderBook(book);
    
    // Add stop orders
    std::cout << "\nAdding stop orders..." << std::endl;
    
    // Stop buy triggered when price rises to 103
    book.processOrder(std::make_shared<StopOrder>(nextOrderId++, "AAPL", Side::BUY, 3, 103));
    
    // Stop limit sell triggered when price falls to 95, with limit of 94
    book.processOrder(std::make_shared<StopLimitOrder>(nextOrderId++, "AAPL", Side::SELL, 4, 94, 95));
    
    printOrderBook(book);
    
    // Execute trades that will trigger stop orders
    std::cout << "\nAdding orders to trigger stops..." << std::endl;
    
    // This trade will set last price to 104, triggering the stop buy
    trades = book.processOrder(std::make_shared<LimitOrder>(nextOrderId++, "AAPL", Side::BUY, 2, 104));
    trades = book.processOrder(std::make_shared<LimitOrder>(nextOrderId++, "AAPL", Side::SELL, 2, 104));
    
    for (const auto& trade : trades) {
        printTrade(trade);
    }
    
    printOrderBook(book);
    
    // This trade will set last price to 94, triggering the stop limit sell
    trades = book.processOrder(std::make_shared<LimitOrder>(nextOrderId++, "AAPL", Side::SELL, 1, 94));
    trades = book.processOrder(std::make_shared<LimitOrder>(nextOrderId++, "AAPL", Side::BUY, 1, 94));
    
    for (const auto& trade : trades) {
        printTrade(trade);
    }
    
    printOrderBook(book);
    
    // Cancel an order
    std::cout << "\nCancelling order id 2..." << std::endl;
    if (book.cancelOrder(2)) {
        std::cout << "Order 2 cancelled successfully" << std::endl;
    }
    
    printOrderBook(book);
    
    // Modify an order
    std::cout << "\nModifying order id 3..." << std::endl;
    if (book.modifyOrder(3, 10, 96)) {
        std::cout << "Order 3 modified successfully" << std::endl;
    }
    
    printOrderBook(book);
}

int main() {
    // Basic example
    orderBookExample();
    
    // Performance benchmark
    std::cout << "\n=== Performance Benchmark ===" << std::endl;
    benchmarkOrderBook(100000);  // Benchmark with 100,000 orders
    
    return 0;
} 