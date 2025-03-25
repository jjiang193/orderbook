#include "include/orderbook/OrderBook.h"
#include <algorithm>
#include <stdexcept>

namespace OrderBook {

OrderBook::OrderBook(const std::string& symbol)
    : symbol_(symbol), lastTradePrice_(0) {
}

std::vector<Trade> OrderBook::processOrder(const OrderPtr& order) {
    // First, validate the order
    if (order->getSymbol() != symbol_) {
        // Order is for a different symbol
        return {};
    }

    if (order->getStatus() == OrderStatus::REJECTED) {
        // Order has been rejected
        return {};
    }

    // Add order to our map
    {
        std::lock_guard<std::mutex> lock(ordersMutex_);
        orders_[order->getId()] = order;
    }

    // Process based on order type
    switch(order->getType()) {
        case OrderType::MARKET:
            return processMarketOrder(order);
        
        case OrderType::LIMIT:
            return processLimitOrder(order);
        
        case OrderType::STOP:
        case OrderType::STOP_LIMIT:
            // Add to stop order list
            {
                std::lock_guard<std::mutex> lock(stopOrdersMutex_);
                stopOrders_.push_back(order);
            }
            
            // Check if the stop order should be triggered immediately
            if (lastTradePrice_ != 0) {
                if (order->checkStopTrigger(lastTradePrice_)) {
                    // If triggered, process as market/limit order
                    if (order->getType() == OrderType::STOP) {
                        return processMarketOrder(order);
                    } else {  // STOP_LIMIT
                        return processLimitOrder(order);
                    }
                }
            }
            return {};
        
        default:
            return {};
    }
}

std::vector<Trade> OrderBook::processMarketOrder(const OrderPtr& order) {
    // Market orders are not added to the book, they are matched immediately
    std::vector<Trade> trades = matchOrder(order);
    
    // If market order could not be fully filled, it is considered filled
    // as market orders execute at whatever quantity is available
    if (order->getRemainingQuantity() > 0) {
        // For market orders, we consider them filled even if partially filled
        order->cancel();
    }
    
    // Update last trade price if trades occurred
    if (!trades.empty()) {
        lastTradePrice_ = trades.back().price;
        
        // Check for triggered stop orders
        auto triggerTrades = processTriggerOrders(lastTradePrice_);
        trades.insert(trades.end(), triggerTrades.begin(), triggerTrades.end());
    }
    
    return trades;
}

std::vector<Trade> OrderBook::processLimitOrder(const OrderPtr& order) {
    // Limit orders can be matched against existing orders
    std::vector<Trade> trades = matchOrder(order);
    
    // If there's remaining quantity, add to the book
    if (order->getRemainingQuantity() > 0 && order->isActive()) {
        addToBook(order);
    }
    
    // Update last trade price if trades occurred
    if (!trades.empty()) {
        lastTradePrice_ = trades.back().price;
        
        // Check for triggered stop orders
        auto triggerTrades = processTriggerOrders(lastTradePrice_);
        trades.insert(trades.end(), triggerTrades.begin(), triggerTrades.end());
    }
    
    return trades;
}

std::vector<Trade> OrderBook::matchOrder(const OrderPtr& order) {
    std::vector<Trade> trades;
    
    // Market orders have no price limit, we set to infinity or zero
    // depending on the side, but we don't actually use this variable
    // as the limit checking happens in the matchAgainstBook method
    if (order->getType() == OrderType::MARKET) {
        // Just initialize for clarity, but we don't need to use it
        // since the check is done inside matchAgainstBook
    }
    
    // Match against the appropriate side of the book
    if (order->getSide() == Side::BUY) {
        auto matchedTrades = matchAgainstBook(order, sellLevels_, sellMutex_);
        trades.insert(trades.end(), matchedTrades.begin(), matchedTrades.end());
    } else {  // SELL
        auto matchedTrades = matchAgainstBook(order, buyLevels_, buyMutex_);
        trades.insert(trades.end(), matchedTrades.begin(), matchedTrades.end());
    }
    
    return trades;
}

std::vector<Trade> OrderBook::matchAgainstBook(
    const OrderPtr& order,
    std::map<Price, PriceLevel, BuyCompare>& levels,
    std::mutex& levelsMutex) {
    
    std::vector<Trade> trades;
    
    // Buy order matches against sell levels
    if (order->getSide() == Side::BUY) {
        std::lock_guard<std::mutex> lock(levelsMutex);
        
        for (auto it = levels.begin(); it != levels.end() && order->getRemainingQuantity() > 0;) {
            // Check if price is acceptable
            if (it->first > order->getPrice() && order->getType() != OrderType::MARKET) {
                break;  // Price too high for buy order
            }
            
            PriceLevel& level = it->second;
            
            // Match against orders at this level (front to back, FIFO)
            while (!level.orders.empty() && order->getRemainingQuantity() > 0) {
                OrderPtr& matchOrder = level.orders.front();
                
                // Calculate trade quantity
                Quantity tradeQty = std::min(order->getRemainingQuantity(), 
                                           matchOrder->getRemainingQuantity());
                
                // Execute the trade
                if (matchOrder->fill(tradeQty, it->first) && order->fill(tradeQty, it->first)) {
                    // Create and record the trade
                    trades.push_back(createTrade(order, matchOrder, tradeQty, it->first));
                    
                    // Update level quantity
                    level.totalQuantity -= tradeQty;
                    
                    // If matched order is filled, remove it
                    if (matchOrder->isFilled()) {
                        level.orders.pop_front();
                    }
                }
            }
            
            // If level is now empty, remove it
            if (level.isEmpty()) {
                it = levels.erase(it);
            } else {
                ++it;
            }
        }
    }
    // Sell order matches against buy levels
    else {
        std::lock_guard<std::mutex> lock(levelsMutex);
        
        for (auto it = levels.begin(); it != levels.end() && order->getRemainingQuantity() > 0;) {
            // Check if price is acceptable
            if (it->first < order->getPrice() && order->getType() != OrderType::MARKET) {
                break;  // Price too low for sell order
            }
            
            PriceLevel& level = it->second;
            
            // Match against orders at this level (front to back, FIFO)
            while (!level.orders.empty() && order->getRemainingQuantity() > 0) {
                OrderPtr& matchOrder = level.orders.front();
                
                // Calculate trade quantity
                Quantity tradeQty = std::min(order->getRemainingQuantity(), 
                                           matchOrder->getRemainingQuantity());
                
                // Execute the trade
                if (matchOrder->fill(tradeQty, it->first) && order->fill(tradeQty, it->first)) {
                    // Create and record the trade
                    trades.push_back(createTrade(matchOrder, order, tradeQty, it->first));
                    
                    // Update level quantity
                    level.totalQuantity -= tradeQty;
                    
                    // If matched order is filled, remove it
                    if (matchOrder->isFilled()) {
                        level.orders.pop_front();
                    }
                }
            }
            
            // If level is now empty, remove it
            if (level.isEmpty()) {
                it = levels.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    return trades;
}

std::vector<Trade> OrderBook::matchAgainstBook(
    const OrderPtr& order,
    std::map<Price, PriceLevel, SellCompare>& levels,
    std::mutex& levelsMutex) {
    
    std::vector<Trade> trades;
    
    // Sell side has lowest price first
    std::lock_guard<std::mutex> lock(levelsMutex);
    
    for (auto it = levels.begin(); it != levels.end() && order->getRemainingQuantity() > 0;) {
        // Check if price is acceptable for a buy order
        if (it->first > order->getPrice() && order->getType() != OrderType::MARKET) {
            break;  // Price too high for buy order
        }
        
        PriceLevel& level = it->second;
        
        // Match against orders at this level (front to back, FIFO)
        while (!level.orders.empty() && order->getRemainingQuantity() > 0) {
            OrderPtr& matchOrder = level.orders.front();
            
            // Calculate trade quantity
            Quantity tradeQty = std::min(order->getRemainingQuantity(), 
                                        matchOrder->getRemainingQuantity());
            
            // Execute the trade
            if (matchOrder->fill(tradeQty, it->first) && order->fill(tradeQty, it->first)) {
                // Create and record the trade
                trades.push_back(createTrade(order, matchOrder, tradeQty, it->first));
                
                // Update level quantity
                level.totalQuantity -= tradeQty;
                
                // If matched order is filled, remove it
                if (matchOrder->isFilled()) {
                    level.orders.pop_front();
                }
            }
        }
        
        // If level is now empty, remove it
        if (level.isEmpty()) {
            it = levels.erase(it);
        } else {
            ++it;
        }
    }
    
    return trades;
}

Trade OrderBook::createTrade(
    const OrderPtr& buyOrder, 
    const OrderPtr& sellOrder, 
    Quantity quantity, 
    Price price) {
    return Trade(buyOrder->getId(), sellOrder->getId(), symbol_, quantity, price);
}

void OrderBook::addToBook(const OrderPtr& order) {
    if (order->getSide() == Side::BUY) {
        std::lock_guard<std::mutex> lock(buyMutex_);
        auto& level = buyLevels_[order->getPrice()];
        if (level.price == 0) {
            level.price = order->getPrice();
        }
        level.addOrder(order);
    } else {  // SELL
        std::lock_guard<std::mutex> lock(sellMutex_);
        auto& level = sellLevels_[order->getPrice()];
        if (level.price == 0) {
            level.price = order->getPrice();
        }
        level.addOrder(order);
    }
}

void OrderBook::removeFromBook(const OrderPtr& order) {
    if (order->getSide() == Side::BUY) {
        std::lock_guard<std::mutex> lock(buyMutex_);
        auto it = buyLevels_.find(order->getPrice());
        if (it != buyLevels_.end()) {
            it->second.removeOrder(order);
            if (it->second.isEmpty()) {
                buyLevels_.erase(it);
            }
        }
    } else {  // SELL
        std::lock_guard<std::mutex> lock(sellMutex_);
        auto it = sellLevels_.find(order->getPrice());
        if (it != sellLevels_.end()) {
            it->second.removeOrder(order);
            if (it->second.isEmpty()) {
                sellLevels_.erase(it);
            }
        }
    }
}

bool OrderBook::cancelOrder(OrderId orderId) {
    // Find the order
    OrderPtr order;
    {
        std::lock_guard<std::mutex> lock(ordersMutex_);
        auto it = orders_.find(orderId);
        if (it == orders_.end()) {
            return false;  // Order not found
        }
        order = it->second;
    }
    
    if (order->isTriggerOrder() && !order->isTriggered()) {
        // Remove from stop orders list
        std::lock_guard<std::mutex> lock(stopOrdersMutex_);
        auto it = std::find_if(stopOrders_.begin(), stopOrders_.end(),
                              [orderId](const OrderPtr& o) { return o->getId() == orderId; });
        if (it != stopOrders_.end()) {
            stopOrders_.erase(it);
        }
    } else if (order->isActive()) {
        // Remove from book
        removeFromBook(order);
    }
    
    // Cancel the order
    order->cancel();
    return true;
}

bool OrderBook::modifyOrder(OrderId orderId, Quantity newQuantity, Price newPrice, Price newStopPrice) {
    // Find the order
    OrderPtr order;
    {
        std::lock_guard<std::mutex> lock(ordersMutex_);
        auto it = orders_.find(orderId);
        if (it == orders_.end()) {
            return false;  // Order not found
        }
        order = it->second;
    }
    
    // If the order is in the book, remove it first
    if (order->isActive() && !order->isTriggerOrder()) {
        removeFromBook(order);
    } else if (order->isTriggerOrder() && !order->isTriggered()) {
        // For stop orders that haven't been triggered, we don't need to 
        // remove from the book, but we do need to check if the new stop price
        // would trigger the order
        newStopPrice = (newStopPrice == PRICE_ZERO) ? order->getStopPrice() : newStopPrice;
    }
    
    // Modify the order
    if (!order->modify(newQuantity, newPrice, newStopPrice)) {
        // If modification failed, re-add the original order to the book
        if (order->isActive() && !order->isTriggerOrder()) {
            addToBook(order);
        }
        return false;
    }
    
    // If the order is still active, add it back to the book with the new details
    if (order->isActive() && !order->isTriggerOrder()) {
        addToBook(order);
    } else if (order->isTriggerOrder() && !order->isTriggered()) {
        // For stop orders, check if the new stop price would trigger them
        if (lastTradePrice_ != 0 && order->checkStopTrigger(lastTradePrice_)) {
            // Process the triggered order
            auto trades = (order->getType() == OrderType::STOP) ? 
                        processMarketOrder(order) : processLimitOrder(order);
        }
    }
    
    return true;
}

OrderPtr OrderBook::getOrder(OrderId orderId) const {
    std::lock_guard<std::mutex> lock(ordersMutex_);
    auto it = orders_.find(orderId);
    if (it == orders_.end()) {
        return nullptr;
    }
    return it->second;
}

Price OrderBook::getBestBid() const {
    std::lock_guard<std::mutex> lock(buyMutex_);
    if (buyLevels_.empty()) {
        return 0;
    }
    return buyLevels_.begin()->first;
}

Price OrderBook::getBestAsk() const {
    std::lock_guard<std::mutex> lock(sellMutex_);
    if (sellLevels_.empty()) {
        return PRICE_INFINITY;
    }
    return sellLevels_.begin()->first;
}

Quantity OrderBook::getVolumeAtPrice(Side side, Price price) const {
    if (side == Side::BUY) {
        std::lock_guard<std::mutex> lock(buyMutex_);
        auto it = buyLevels_.find(price);
        if (it != buyLevels_.end()) {
            return it->second.totalQuantity;
        }
    } else {  // SELL
        std::lock_guard<std::mutex> lock(sellMutex_);
        auto it = sellLevels_.find(price);
        if (it != sellLevels_.end()) {
            return it->second.totalQuantity;
        }
    }
    return 0;
}

bool OrderBook::isEmpty() const {
    std::lock_guard<std::mutex> buyLock(buyMutex_);
    std::lock_guard<std::mutex> sellLock(sellMutex_);
    return buyLevels_.empty() && sellLevels_.empty();
}

std::vector<Trade> OrderBook::processTriggerOrders(Price lastTradePrice) {
    std::vector<OrderPtr> triggeredOrders;
    
    // Find all stop orders that should be triggered
    {
        std::lock_guard<std::mutex> lock(stopOrdersMutex_);
        auto it = stopOrders_.begin();
        while (it != stopOrders_.end()) {
            if ((*it)->checkStopTrigger(lastTradePrice)) {
                triggeredOrders.push_back(*it);
                it = stopOrders_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // Process all triggered orders
    std::vector<Trade> trades;
    for (const auto& order : triggeredOrders) {
        std::vector<Trade> orderTrades;
        if (order->getType() == OrderType::STOP) {
            orderTrades = processMarketOrder(order);
        } else {  // STOP_LIMIT
            orderTrades = processLimitOrder(order);
        }
        trades.insert(trades.end(), orderTrades.begin(), orderTrades.end());
    }
    
    return trades;
}

} // namespace OrderBook 