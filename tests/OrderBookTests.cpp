#include "include/orderbook/OrderBook.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>

using namespace OrderBook;

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook book{"TEST"};
    OrderId nextOrderId = 1;

    // Helper to create orders
    OrderPtr createLimitBuy(Quantity qty, Price price) {
        return std::make_shared<LimitOrder>(nextOrderId++, "TEST", Side::BUY, qty, price);
    }

    OrderPtr createLimitSell(Quantity qty, Price price) {
        return std::make_shared<LimitOrder>(nextOrderId++, "TEST", Side::SELL, qty, price);
    }

    OrderPtr createMarketBuy(Quantity qty) {
        return std::make_shared<MarketOrder>(nextOrderId++, "TEST", Side::BUY, qty);
    }

    OrderPtr createMarketSell(Quantity qty) {
        return std::make_shared<MarketOrder>(nextOrderId++, "TEST", Side::SELL, qty);
    }

    OrderPtr createStopBuy(Quantity qty, Price stopPrice) {
        return std::make_shared<StopOrder>(nextOrderId++, "TEST", Side::BUY, qty, stopPrice);
    }

    OrderPtr createStopSell(Quantity qty, Price stopPrice) {
        return std::make_shared<StopOrder>(nextOrderId++, "TEST", Side::SELL, qty, stopPrice);
    }

    OrderPtr createStopLimitBuy(Quantity qty, Price price, Price stopPrice) {
        return std::make_shared<StopLimitOrder>(nextOrderId++, "TEST", Side::BUY, qty, price, stopPrice);
    }

    OrderPtr createStopLimitSell(Quantity qty, Price price, Price stopPrice) {
        return std::make_shared<StopLimitOrder>(nextOrderId++, "TEST", Side::SELL, qty, price, stopPrice);
    }
};

// Test basic limit order addition to the book
TEST_F(OrderBookTest, AddLimitOrders) {
    // Add buy orders
    book.processOrder(createLimitBuy(10, 95));
    book.processOrder(createLimitBuy(5, 100));
    book.processOrder(createLimitBuy(7, 97));
    
    EXPECT_EQ(book.getBestBid(), 100);
    
    // Add sell orders
    book.processOrder(createLimitSell(8, 105));
    book.processOrder(createLimitSell(3, 103));
    book.processOrder(createLimitSell(5, 110));
    
    EXPECT_EQ(book.getBestAsk(), 103);
}

// Test market order matching
TEST_F(OrderBookTest, MarketOrderMatching) {
    // Add some limit orders
    book.processOrder(createLimitBuy(10, 95));
    book.processOrder(createLimitBuy(5, 100));
    book.processOrder(createLimitSell(8, 105));
    book.processOrder(createLimitSell(3, 103));
    
    // Market buy should match against lowest sell (103)
    auto trades = book.processOrder(createMarketBuy(2));
    
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 2);
    EXPECT_EQ(trades[0].price, 103);
    
    // Only 1 unit left at 103
    EXPECT_EQ(book.getVolumeAtPrice(Side::SELL, 103), 1);
    
    // Market sell should match against highest buy (100)
    trades = book.processOrder(createMarketSell(3));
    
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 3);
    EXPECT_EQ(trades[0].price, 100);
    
    // Only 2 units left at 100
    EXPECT_EQ(book.getVolumeAtPrice(Side::BUY, 100), 2);
}

// Test crossing limit orders
TEST_F(OrderBookTest, CrossingLimitOrders) {
    // Add some limit orders
    book.processOrder(createLimitBuy(5, 100));
    book.processOrder(createLimitSell(3, 103));
    book.processOrder(createLimitSell(8, 105));
    
    // Limit buy that crosses the book
    auto trades = book.processOrder(createLimitBuy(4, 104));
    
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 3);
    EXPECT_EQ(trades[0].price, 103);
    
    // 1 unit should be left in the buy order at 104
    EXPECT_EQ(book.getVolumeAtPrice(Side::BUY, 104), 1);
    EXPECT_EQ(book.getBestBid(), 104);
    
    // Limit sell that crosses the book
    trades = book.processOrder(createLimitSell(7, 99));
    
    ASSERT_EQ(trades.size(), 2);
    EXPECT_EQ(trades[0].quantity, 1);
    EXPECT_EQ(trades[0].price, 104);
    EXPECT_EQ(trades[1].quantity, 5);
    EXPECT_EQ(trades[1].price, 100);
    
    // 1 unit should be left in the sell order at 99
    EXPECT_EQ(book.getVolumeAtPrice(Side::SELL, 99), 1);
    EXPECT_EQ(book.getBestAsk(), 99);
}

// Test order cancellation
TEST_F(OrderBookTest, OrderCancellation) {
    // Add a buy order
    OrderPtr order = createLimitBuy(10, 100);
    book.processOrder(order);
    
    EXPECT_EQ(book.getVolumeAtPrice(Side::BUY, 100), 10);
    
    // Cancel the order
    EXPECT_TRUE(book.cancelOrder(order->getId()));
    
    // Order should be removed from book
    EXPECT_EQ(book.getVolumeAtPrice(Side::BUY, 100), 0);
    
    // Check order status
    auto cancelledOrder = book.getOrder(order->getId());
    EXPECT_TRUE(cancelledOrder->isCancelled());
}

// Test order modification
TEST_F(OrderBookTest, OrderModification) {
    // Add a buy order
    OrderPtr order = createLimitBuy(10, 100);
    book.processOrder(order);
    
    EXPECT_EQ(book.getVolumeAtPrice(Side::BUY, 100), 10);
    
    // Modify the order
    EXPECT_TRUE(book.modifyOrder(order->getId(), 15, 102));
    
    // Old price level should be removed
    EXPECT_EQ(book.getVolumeAtPrice(Side::BUY, 100), 0);
    
    // New price level should be added
    EXPECT_EQ(book.getVolumeAtPrice(Side::BUY, 102), 15);
    EXPECT_EQ(book.getBestBid(), 102);
}

// Test stop order triggering
TEST_F(OrderBookTest, StopOrderTriggering) {
    // Add a stop buy order that triggers at 105
    book.processOrder(createStopBuy(10, 105));
    
    // Add sell orders
    book.processOrder(createLimitSell(5, 110));
    
    // Create a trade at 105 to trigger the stop order
    auto trades = book.processOrder(createLimitBuy(2, 110));
    
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].price, 110);
    
    // Stop order should now be a market order and executed
    ASSERT_EQ(trades.size(), 1);
    
    // Remaining quantity in sell order
    EXPECT_EQ(book.getVolumeAtPrice(Side::SELL, 110), 3);
    
    // Add a stop sell order that triggers at 95
    book.processOrder(createStopSell(7, 95));
    
    // Add buy orders
    book.processOrder(createLimitBuy(10, 90));
    
    // Create a trade at 95 to trigger the stop order
    trades = book.processOrder(createLimitSell(3, 90));
    
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].price, 90);
    
    // Stop order should now be a market order and executed
    ASSERT_EQ(trades.size(), 1);
    
    // Remaining quantity in buy order
    EXPECT_EQ(book.getVolumeAtPrice(Side::BUY, 90), 7);
}

// Test stop limit order triggering
TEST_F(OrderBookTest, StopLimitOrderTriggering) {
    // Add a stop limit buy order that triggers at 105, with limit 107
    book.processOrder(createStopLimitBuy(10, 107, 105));
    
    // Add sell orders
    book.processOrder(createLimitSell(5, 106));
    book.processOrder(createLimitSell(8, 108));
    
    // Create a trade at 105 to trigger the stop limit order
    auto trades = book.processOrder(createLimitBuy(2, 105));
    book.processOrder(createLimitSell(2, 105));
    
    // Stop limit order should now be triggered and matched against the 106 sell
    EXPECT_GT(trades.size(), 0);
    
    // Buy with limit of 107 should match sell at 106
    bool foundMatch = false;
    for (const auto& trade : trades) {
        if (trade.price == 106 && trade.quantity == 5) {
            foundMatch = true;
            break;
        }
    }
    EXPECT_TRUE(foundMatch);
    
    // 5 units should remain in the buy order (10 - 5)
    // but it won't match the 108 sell because the limit is 107
    EXPECT_EQ(book.getVolumeAtPrice(Side::BUY, 107), 5);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 