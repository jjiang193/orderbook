#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <memory>
#include <variant>
#include <optional>
#include <tuple>
#include <format>
#include <numeric>
#include <vector>
#include <queue>
#include <deque>
#include <map>
#include <set>
#include <list>
#include <cmath>
#include <stack>
#include <limits>
#include <string>



enum class OrderType {goodTillCancel, fillAndKill};

enum class Side{buy, sell};

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderID = std::uint32_t;

struct LevelInfo{
    Price price_;
    Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;


class OrderBookLevelInfos{
    public:
        OrderBookLevelInfos(const LevelInfos& bids, const LevelInfos& asks)
            : bids_{bids}, asks_{asks}
        {}

        const LevelInfos& GetBids() const{
            return bids_;
        }
        const LevelInfos& GetAsks() const{
            return asks_;
        }
    
    private:
        LevelInfos bids_;
        LevelInfos asks_;
};


class Order{
    public:
        Order(OrderType orderType, OrderID orderId, Side side, Price price, Quantity quantity)
            : orderType_{orderType}, orderId_{orderId}, side_{side}, price_{price}, initialQuantity_{quantity}, remainingQuantity_{quantity}
        {}

        OrderID GetOrderID() const{
            return orderId_;
        }
        Side GetSide() const{
            return side_;
        }
        Price GetPrice() const{
            return price_;
        }
        Quantity GetInitialQuantity() const{
            return initialQuantity_;
        }
        Quantity GetRemainingQuantity() const{
            return remainingQuantity_;
        }
        Quantity GetFilledQuantity() const{
            return initialQuantity_ - remainingQuantity_;
        }
        void Fill(Quantity quantity){
            if (quantity > remainingQuantity_){
                throw std::logic_error("Fill quantity is greater than remaining quantity");
            }
            remainingQuantity_ -= quantity;
        }
    private:
        OrderType orderType_;
        OrderID orderId_;
        Side side_;
        Price price_;
        Quantity initialQuantity_;
        Quantity remainingQuantity_;
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

class OrderModification{
    public:
        OrderModification(OrderID orderID, Side side, Price price, Quantity quantity) : orderId_{orderID}, side_{side}, price_{price}, quantity_{quantity}
        {}

        OrderID GetOrderID() const{
            return orderId_;
        }
        Side GetSide() const{
            return side_;
        }
        Price GetPrice() const{
            return price_;
        }
        Quantity GetQuantity() const{
            return quantity_;
        }

        OrderPointer ToOrderPointer(OrderType type) const {
            return std::make_shared<Order>(type, GetOrderID(), GetSide(), GetPrice(), GetQuantity());
        }
    private:
        OrderID orderId_;
        Side side_;
        Price price_;
        Quantity quantity_;
};


struct TradeInfo{
    OrderID orderID_;
    Quantity quantity_;
    Price price_;
};

class Trade{
    public:
        Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade)
            : bidTrade_{bidTrade}, askTrade_{askTrade}
        {}

        const TradeInfo& GetBidTrade() const{
            return bidTrade_;
        }
        const TradeInfo& GetAskTrade() const{
            return askTrade_;
        }
    private:
        TradeInfo bidTrade_;
        TradeInfo askTrade_;
};

using Trades = std::vector<Trade>;

class OrderBook{
    private:
        struct OrderEntry{
            OrderPointer order_{nullptr};
            OrderPointers::iterator location_;
        };
        std::map<Price, OrderPointers, std::greater<Price>> bids_;
        std::map<Price, OrderPointers, std::less<Price>> asks_;
        std::unordered_map<OrderID, OrderEntry> orders_;

        
}