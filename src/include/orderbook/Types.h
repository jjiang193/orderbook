#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <limits>

namespace OrderBook {

// Order ID type
using OrderId = uint64_t;

// Price representation (using integer to avoid floating point issues)
using Price = int64_t;

// Quantity representation
using Quantity = uint64_t;

// Time representation
using Timestamp = std::chrono::time_point<std::chrono::high_resolution_clock>;

// Side of the order
enum class Side {
    BUY,
    SELL
};

// Order types
enum class OrderType {
    MARKET,         // Execute at best available price
    LIMIT,          // Execute at specific price or better
    STOP,           // Becomes market order when market price reaches stop price
    STOP_LIMIT      // Becomes limit order when market price reaches stop price
};

// Order status
enum class OrderStatus {
    NEW,            // Order has been accepted but not yet processed
    ACTIVE,         // Order is active in the book
    PARTIALLY_FILLED, // Order has been partially filled
    FILLED,         // Order has been completely filled
    CANCELLED,      // Order has been cancelled
    REJECTED        // Order has been rejected
};

// Trading action
enum class TradingAction {
    ADD,            // Add a new order
    MODIFY,         // Modify an existing order
    CANCEL          // Cancel an existing order
};

// Constants
constexpr Price PRICE_INFINITY = std::numeric_limits<Price>::max();
constexpr Price PRICE_ZERO = 0;
constexpr Quantity QUANTITY_ZERO = 0;

} // namespace OrderBook 