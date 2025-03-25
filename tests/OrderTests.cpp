#include "include/orderbook/Order.h"
#include <gtest/gtest.h>

using namespace OrderBook;

// Test Order constructor and initial state
TEST(OrderTest, Constructor) {
    // Create a limit order
    LimitOrder order(1, "AAPL", Side::BUY, 100, 50);
    
    // Check properties
    EXPECT_EQ(order.getId(), 1);
    EXPECT_EQ(order.getSymbol(), "AAPL");
    EXPECT_EQ(order.getSide(), Side::BUY);
    EXPECT_EQ(order.getType(), OrderType::LIMIT);
    EXPECT_EQ(order.getQuantity(), 100);
    EXPECT_EQ(order.getFilledQuantity(), 0);
    EXPECT_EQ(order.getRemainingQuantity(), 100);
    EXPECT_EQ(order.getPrice(), 50);
    EXPECT_EQ(order.getStatus(), OrderStatus::ACTIVE);
}

// Test invalid order rejection
TEST(OrderTest, InvalidOrderRejection) {
    // Order with zero quantity should be rejected
    LimitOrder zeroQtyOrder(1, "AAPL", Side::BUY, 0, 50);
    EXPECT_EQ(zeroQtyOrder.getStatus(), OrderStatus::REJECTED);
    
    // Limit order with zero price should be rejected
    LimitOrder zeroPriceOrder(2, "AAPL", Side::BUY, 100, 0);
    EXPECT_EQ(zeroPriceOrder.getStatus(), OrderStatus::REJECTED);
    
    // Stop order with zero stop price should be rejected
    StopOrder zeroStopOrder(3, "AAPL", Side::BUY, 100, 0);
    EXPECT_EQ(zeroStopOrder.getStatus(), OrderStatus::REJECTED);
}

// Test order filling
TEST(OrderTest, OrderFilling) {
    LimitOrder order(1, "AAPL", Side::BUY, 100, 50);
    
    // Partial fill
    EXPECT_TRUE(order.fill(30, 50));
    EXPECT_EQ(order.getFilledQuantity(), 30);
    EXPECT_EQ(order.getRemainingQuantity(), 70);
    EXPECT_EQ(order.getStatus(), OrderStatus::PARTIALLY_FILLED);
    
    // Complete fill
    EXPECT_TRUE(order.fill(70, 50));
    EXPECT_EQ(order.getFilledQuantity(), 100);
    EXPECT_EQ(order.getRemainingQuantity(), 0);
    EXPECT_EQ(order.getStatus(), OrderStatus::FILLED);
    
    // Cannot fill more than available
    EXPECT_FALSE(order.fill(10, 50));
}

// Test order cancellation
TEST(OrderTest, OrderCancellation) {
    LimitOrder order(1, "AAPL", Side::BUY, 100, 50);
    
    EXPECT_TRUE(order.isActive());
    order.cancel();
    EXPECT_FALSE(order.isActive());
    EXPECT_TRUE(order.isCancelled());
}

// Test order modification
TEST(OrderTest, OrderModification) {
    LimitOrder order(1, "AAPL", Side::BUY, 100, 50);
    
    // Valid modification
    EXPECT_TRUE(order.modify(150, 55));
    EXPECT_EQ(order.getQuantity(), 150);
    EXPECT_EQ(order.getPrice(), 55);
    
    // Partially fill
    order.fill(50, 55);
    
    // Cannot reduce quantity below filled
    EXPECT_FALSE(order.modify(40, 55));
    
    // Can increase or keep quantity same
    EXPECT_TRUE(order.modify(50, 60));
    EXPECT_EQ(order.getQuantity(), 50);
    EXPECT_EQ(order.getPrice(), 60);
    
    // Cancel the order
    order.cancel();
    
    // Cannot modify cancelled order
    EXPECT_FALSE(order.modify(70, 65));
}

// Test stop order triggering
TEST(OrderTest, StopOrderTriggering) {
    // BUY stop (triggers when price rises above stop price)
    StopOrder buyStop(1, "AAPL", Side::BUY, 100, 105);
    
    // Should not trigger yet
    EXPECT_FALSE(buyStop.checkStopTrigger(100));
    EXPECT_FALSE(buyStop.isTriggered());
    
    // Should trigger
    EXPECT_TRUE(buyStop.checkStopTrigger(105));
    EXPECT_TRUE(buyStop.isTriggered());
    
    // Already triggered, should not trigger again
    EXPECT_FALSE(buyStop.checkStopTrigger(110));
    
    // SELL stop (triggers when price falls below stop price)
    StopOrder sellStop(2, "AAPL", Side::SELL, 100, 95);
    
    // Should not trigger yet
    EXPECT_FALSE(sellStop.checkStopTrigger(100));
    EXPECT_FALSE(sellStop.isTriggered());
    
    // Should trigger
    EXPECT_TRUE(sellStop.checkStopTrigger(95));
    EXPECT_TRUE(sellStop.isTriggered());
    
    // Already triggered, should not trigger again
    EXPECT_FALSE(sellStop.checkStopTrigger(90));
}

// Test stop limit order triggering
TEST(OrderTest, StopLimitOrderTriggering) {
    // BUY stop limit (triggers when price rises above stop price)
    StopLimitOrder buyStopLimit(1, "AAPL", Side::BUY, 100, 100, 105);
    
    // Should not trigger yet
    EXPECT_FALSE(buyStopLimit.checkStopTrigger(100));
    EXPECT_FALSE(buyStopLimit.isTriggered());
    
    // Should trigger
    EXPECT_TRUE(buyStopLimit.checkStopTrigger(105));
    EXPECT_TRUE(buyStopLimit.isTriggered());
    
    // SELL stop limit (triggers when price falls below stop price)
    StopLimitOrder sellStopLimit(2, "AAPL", Side::SELL, 100, 90, 95);
    
    // Should not trigger yet
    EXPECT_FALSE(sellStopLimit.checkStopTrigger(100));
    EXPECT_FALSE(sellStopLimit.isTriggered());
    
    // Should trigger
    EXPECT_TRUE(sellStopLimit.checkStopTrigger(95));
    EXPECT_TRUE(sellStopLimit.isTriggered());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 