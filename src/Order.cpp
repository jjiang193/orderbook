#include "include/orderbook/Order.h"

namespace OrderBook {

Order::Order(OrderId id, 
             const std::string& symbol,
             Side side, 
             OrderType type,
             Quantity quantity, 
             Price price,
             Price stopPrice)
    : id_(id),
      symbol_(symbol),
      side_(side),
      type_(type),
      status_(OrderStatus::NEW),
      quantity_(quantity),
      filledQuantity_(0),
      price_(price),
      stopPrice_(stopPrice),
      timestamp_(std::chrono::high_resolution_clock::now()) {
    
    // Validate the order
    if (quantity == QUANTITY_ZERO) {
        status_ = OrderStatus::REJECTED;
        return;
    }

    // Check price for limit orders
    if ((type == OrderType::LIMIT || type == OrderType::STOP_LIMIT) && price == PRICE_ZERO) {
        status_ = OrderStatus::REJECTED;
        return;
    }

    // Check stop price for stop orders
    if ((type == OrderType::STOP || type == OrderType::STOP_LIMIT) && stopPrice == PRICE_ZERO) {
        status_ = OrderStatus::REJECTED;
        return;
    }

    // Set order as ACTIVE if it's not a triggered order
    if (type != OrderType::STOP && type != OrderType::STOP_LIMIT) {
        status_ = OrderStatus::ACTIVE;
    }
}

bool Order::fill(Quantity quantity, Price /*executionPrice*/) {
    // Cannot fill inactive orders
    if (!isActive()) {
        return false;
    }

    // Cannot fill more than available
    if (quantity > getRemainingQuantity()) {
        return false;
    }

    // Fill the order
    filledQuantity_ += quantity;

    // Update status
    if (filledQuantity_ == quantity_) {
        status_ = OrderStatus::FILLED;
    } else {
        status_ = OrderStatus::PARTIALLY_FILLED;
    }

    return true;
}

void Order::cancel() {
    if (isActive()) {
        status_ = OrderStatus::CANCELLED;
    }
}

bool Order::modify(Quantity newQuantity, Price newPrice, Price newStopPrice) {
    // Cannot modify inactive orders
    if (!isActive()) {
        return false;
    }

    // Cannot reduce quantity below filled quantity
    if (newQuantity < filledQuantity_) {
        return false;
    }

    // Update quantity
    quantity_ = newQuantity;

    // Update prices if applicable
    if (type_ == OrderType::LIMIT || type_ == OrderType::STOP_LIMIT) {
        price_ = newPrice;
    }

    if (type_ == OrderType::STOP || type_ == OrderType::STOP_LIMIT) {
        stopPrice_ = newStopPrice;
    }

    return true;
}

bool StopOrder::checkStopTrigger(Price lastTradePrice) {
    if (triggered_) {
        return false;  // Already triggered
    }

    bool shouldTrigger = false;

    // Trigger logic: 
    // - BUY stop triggers when price rises above or equal to stop price
    // - SELL stop triggers when price falls below or equal to stop price
    if (getSide() == Side::BUY && lastTradePrice >= getStopPrice()) {
        shouldTrigger = true;
    } else if (getSide() == Side::SELL && lastTradePrice <= getStopPrice()) {
        shouldTrigger = true;
    }

    if (shouldTrigger) {
        triggered_ = true;
        return true;
    }

    return false;
}

bool StopLimitOrder::checkStopTrigger(Price lastTradePrice) {
    if (triggered_) {
        return false;  // Already triggered
    }

    bool shouldTrigger = false;

    // Trigger logic: 
    // - BUY stop limit triggers when price rises above or equal to stop price
    // - SELL stop limit triggers when price falls below or equal to stop price
    if (getSide() == Side::BUY && lastTradePrice >= getStopPrice()) {
        shouldTrigger = true;
    } else if (getSide() == Side::SELL && lastTradePrice <= getStopPrice()) {
        shouldTrigger = true;
    }

    if (shouldTrigger) {
        triggered_ = true;
        return true;
    }

    return false;
}

} // namespace OrderBook 