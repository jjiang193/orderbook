#pragma once

#include "Types.h"
#include <string>
#include <memory>

namespace OrderBook {

// Forward declarations
class OrderBook;

// Order class
class Order {
public:
    // Constructor for all order types
    Order(OrderId id, 
          const std::string& symbol,
          Side side, 
          OrderType type,
          Quantity quantity, 
          Price price = PRICE_ZERO,
          Price stopPrice = PRICE_ZERO);

    // Deleted copy constructor and assignment operator (orders should be unique)
    Order(const Order&) = delete;
    Order& operator=(const Order&) = delete;

    // Move constructor and assignment operator
    Order(Order&&) noexcept = default;
    Order& operator=(Order&&) noexcept = default;

    // Virtual destructor for derived classes
    virtual ~Order() = default;

    // Getters
    OrderId getId() const { return id_; }
    const std::string& getSymbol() const { return symbol_; }
    Side getSide() const { return side_; }
    OrderType getType() const { return type_; }
    OrderStatus getStatus() const { return status_; }
    Quantity getQuantity() const { return quantity_; }
    Quantity getFilledQuantity() const { return filledQuantity_; }
    Quantity getRemainingQuantity() const { return quantity_ - filledQuantity_; }
    Price getPrice() const { return price_; }
    Price getStopPrice() const { return stopPrice_; }
    Timestamp getTimestamp() const { return timestamp_; }

    // Status checks
    bool isActive() const { return status_ == OrderStatus::ACTIVE || status_ == OrderStatus::PARTIALLY_FILLED; }
    bool isFilled() const { return status_ == OrderStatus::FILLED; }
    bool isCancelled() const { return status_ == OrderStatus::CANCELLED; }
    bool isRejected() const { return status_ == OrderStatus::REJECTED; }

    // Order operations
    bool fill(Quantity quantity, Price executionPrice);
    void cancel();
    bool modify(Quantity newQuantity, Price newPrice, Price newStopPrice = PRICE_ZERO);

    // Triggered operations for stop orders
    virtual bool checkStopTrigger(Price /*lastTradePrice*/) { return false; }
    virtual bool isTriggerOrder() const { return false; }
    virtual bool isTriggered() const { return false; }

private:
    OrderId id_;                // Unique order ID
    std::string symbol_;        // Trading symbol
    Side side_;                 // BUY or SELL
    OrderType type_;            // Type of order
    OrderStatus status_;        // Current status
    Quantity quantity_;         // Original quantity
    Quantity filledQuantity_;   // Amount that has been filled
    Price price_;               // Limit price (0 for market orders)
    Price stopPrice_;           // Stop price (for STOP and STOP_LIMIT orders)
    Timestamp timestamp_;       // Time when order was created
};

// Smart pointer type for orders
using OrderPtr = std::shared_ptr<Order>;

// Derived classes for specific order types
class MarketOrder : public Order {
public:
    MarketOrder(OrderId id, const std::string& symbol, Side side, Quantity quantity)
        : Order(id, symbol, side, OrderType::MARKET, quantity) {}
};

class LimitOrder : public Order {
public:
    LimitOrder(OrderId id, const std::string& symbol, Side side, Quantity quantity, Price price)
        : Order(id, symbol, side, OrderType::LIMIT, quantity, price) {}
};

class StopOrder : public Order {
public:
    StopOrder(OrderId id, const std::string& symbol, Side side, Quantity quantity, Price stopPrice)
        : Order(id, symbol, side, OrderType::STOP, quantity, PRICE_ZERO, stopPrice),
          triggered_(false) {}

    bool checkStopTrigger(Price lastTradePrice) override;
    bool isTriggerOrder() const override { return true; }
    bool isTriggered() const override { return triggered_; }

private:
    bool triggered_;
};

class StopLimitOrder : public Order {
public:
    StopLimitOrder(OrderId id, const std::string& symbol, Side side, Quantity quantity, 
                  Price price, Price stopPrice)
        : Order(id, symbol, side, OrderType::STOP_LIMIT, quantity, price, stopPrice),
          triggered_(false) {}

    bool checkStopTrigger(Price lastTradePrice) override;
    bool isTriggerOrder() const override { return true; }
    bool isTriggered() const override { return triggered_; }

private:
    bool triggered_;
};

} // namespace OrderBook 