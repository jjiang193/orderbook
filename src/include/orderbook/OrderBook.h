#pragma once

#include "Order.h"
#include <string>
#include <unordered_map>
#include <map>
#include <set>
#include <list>
#include <functional>
#include <mutex>
#include <atomic>
#include <vector>

namespace OrderBook {

// Forward declarations
struct Trade;

// Structure to represent a price level in the order book
struct PriceLevel {
    Price price;
    std::list<OrderPtr> orders;  // Orders at this price level (FIFO queue)
    Quantity totalQuantity;     // Total quantity at this price level
    
    // Default constructor needed for map::operator[]
    PriceLevel() : price(0), totalQuantity(0) {}
    
    PriceLevel(Price p) : price(p), totalQuantity(0) {}
    
    void addOrder(const OrderPtr& order) {
        orders.push_back(order);
        totalQuantity += order->getRemainingQuantity();
    }
    
    void removeOrder(const OrderPtr& order) {
        auto it = std::find_if(orders.begin(), orders.end(), 
                              [&order](const OrderPtr& o) { return o->getId() == order->getId(); });
        if (it != orders.end()) {
            totalQuantity -= (*it)->getRemainingQuantity();
            orders.erase(it);
        }
    }
    
    bool isEmpty() const {
        return orders.empty();
    }
};

// Trade structure
struct Trade {
    OrderId buyOrderId;
    OrderId sellOrderId;
    std::string symbol;
    Quantity quantity;
    Price price;
    Timestamp timestamp;
    
    Trade(OrderId buyId, OrderId sellId, const std::string& sym, 
          Quantity qty, Price p)
        : buyOrderId(buyId), sellOrderId(sellId), symbol(sym),
          quantity(qty), price(p), timestamp(std::chrono::high_resolution_clock::now()) {}
};

// Comparator for buy side (highest price first)
struct BuyCompare {
    bool operator()(const Price& lhs, const Price& rhs) const {
        return lhs > rhs;  // Descending order for buy side
    }
};

// Comparator for sell side (lowest price first)
struct SellCompare {
    bool operator()(const Price& lhs, const Price& rhs) const {
        return lhs < rhs;  // Ascending order for sell side
    }
};

// Order Book class
class OrderBook {
public:
    // Constructor
    OrderBook(const std::string& symbol);
    
    // Deleted copy constructor and assignment operator
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    
    // Process different types of orders
    std::vector<Trade> processOrder(const OrderPtr& order);
    std::vector<Trade> processMarketOrder(const OrderPtr& order);
    std::vector<Trade> processLimitOrder(const OrderPtr& order);
    
    // Order management
    bool cancelOrder(OrderId orderId);
    bool modifyOrder(OrderId orderId, Quantity newQuantity, Price newPrice, Price newStopPrice = PRICE_ZERO);
    
    // Market data access
    Price getBestBid() const;
    Price getBestAsk() const;
    Quantity getVolumeAtPrice(Side side, Price price) const;
    
    // Market status check
    bool isEmpty() const;
    std::string getSymbol() const { return symbol_; }
    
    // Get order from ID
    OrderPtr getOrder(OrderId orderId) const;
    
    // Process triggered orders (for stop orders)
    std::vector<Trade> processTriggerOrders(Price lastTradePrice);
    
    // Add an existing order to the book
    void addToBook(const OrderPtr& order);
    
    // Remove an order from the book
    void removeFromBook(const OrderPtr& order);
    
private:
    std::string symbol_;                             // Trading symbol
    std::atomic<Price> lastTradePrice_;              // Last trade price
    
    // Order storage
    mutable std::mutex ordersMutex_;
    std::unordered_map<OrderId, OrderPtr> orders_;   // All orders by ID
    
    // Buy side order book (price levels sorted by price, highest first)
    mutable std::mutex buyMutex_;
    std::map<Price, PriceLevel, BuyCompare> buyLevels_;
    
    // Sell side order book (price levels sorted by price, lowest first)
    mutable std::mutex sellMutex_;
    std::map<Price, PriceLevel, SellCompare> sellLevels_;
    
    // Stop order containers
    mutable std::mutex stopOrdersMutex_;
    std::list<OrderPtr> stopOrders_;                 // All stop orders
    
    // Match a single order against the book
    std::vector<Trade> matchOrder(const OrderPtr& order);
    
    // Match against specific side of the book
    std::vector<Trade> matchAgainstBook(const OrderPtr& order, 
                                      std::map<Price, PriceLevel, BuyCompare>& levels,
                                      std::mutex& levelsMutex);
    std::vector<Trade> matchAgainstBook(const OrderPtr& order, 
                                      std::map<Price, PriceLevel, SellCompare>& levels,
                                      std::mutex& levelsMutex);
    
    // Check if an order can be matched against a price level
    bool canMatch(const OrderPtr& order, Price price) const;
    
    // Create a trade between two orders
    Trade createTrade(const OrderPtr& buyOrder, const OrderPtr& sellOrder, 
                     Quantity quantity, Price price);
};

} // namespace OrderBook 