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

enum class Side{Buy, Sell};

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderID = std::uint64_t;

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

        OrderType GetOrderType() const{
            return orderType_;
        }
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
        bool IsFilled() const{
            return GetRemainingQuantity() == 0;
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
        
        bool CanMatch(Side side, Price price) const{
            if (side == Side::buy){
                return !asks_.empty() && asks_.begin()->first <= price;//more efficient than doing another if else first
            }
            return !bids_.empty() && bids_.begin()->first >= price;
        }

        Trades MatchOrders(){
            Trades trades;
            trades.reserve(orders_.size());

            while(true){
                if(bids_.empty() || asks_.empty()){
                    break;
                }

                auto& [bidPrice, bids] = *bids_.begin();
                auto& [askPrice, asks] = *asks_.begin();

                if(bidPrice < askPrice){
                    break;
                }

                while(bids.size() && asks.size()){
                    auto& bid = bids.front();
                    auto& ask = asks.front();

                    Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());
                    bid->Fill(quantity);
                    ask->Fill(quantity);

                    if(bid->IsFilled()){
                        bids.pop_front();
                        orders_.erase(bid->GetOrderID());
                    }

                    if(ask->IsFilled()){
                        asks.pop_front();
                        orders_.erase(ask->GetOrderID());
                    }

                    if(bids.empty()){
                        bids_.erase(bidPrice);
                    }
                    if(asks.empty()){
                        asks_.erase(askPrice);
                    }

                    trades.push_back(Trade{TradeInfo{bid->GetOrderID(), bid->GetPrice(), quantity}, TradeInfo{ask->GetOrderID(), ask->GetPrice(), quantity}});
                }
            }
            if(!bids_.empty()){
                auto& [_, bids] = *bids_.begin();
                auto& order =bids.front();
                if(order->GetOrderType() == OrderType::fillAndKill){
                    CancelOrder(order->GetOrderID());
                }
            
            }
            if(!asks_.empty()){
                auto& [_, asks] = *asks_.begin();
                auto& order = asks.front();
                if(order->GetOrderType() == OrderType::fillAndKill){
                    CancelOrder(order->GetOrderID());
                }
            }
            return trades;
        }
    public:
        
    Trades AddOrder(OrderPointer order){
        if(orders_.contains(order->GetOrderID())){
            return{};
        }
        if(order->GetOrderType()== OrderType::fillAndKill && !CanMatch(order->GetSide(), order->GetPrice())){
            return{};
        }

        OrderPointers::iterator iterator;

        if(order->GetSide() == Side::Buy){
            auto& orders = bids_[order->GetPrice()];
            orders.push_back(order);
            iterator = std::next(orders.begin(), orders.size() - 1);
        }
        else{
            auto& orders = asks_[order->GetPrice()];
            orders.push_back(order);
            iterator = std::next(orders.begin(), orders.size() - 1);
        }

        orders_.insert({order->GetOrderID(), OrderEntry{order, iterator}});
        return MatchOrders();
    }

    void CancelOrder(OrderID orderId){
        if(!orders_.contains(orderId)){
            return;
        }

        const auto& [order, orderIterator] = orders_.at(orderId);
        orders_.erase(orderId);

        if(order->GetSide() == Side::Sell){
            auto price = order->GetPrice();
            auto& orders = asks_.at(price);
            orders.erase(orderIterator);
            if(orders.empty()){
                asks_.erase(price);
            }
        }else{
            auto price = order->GetPrice();
            auto& orders = bids_.at(price);
            orders.erase(orderIterator);
            if(orders.empty()){
                bids_.erase(price);
            }
        }
    }

    Trades MatchOrder(OrderModification order){
        if(!orders_.contains(order.GetOrderID())){
            return{};
        }
        const auto& [exisitingOrder, _] = orders_.at(order.GetOrderID());
        CancelOrder(order.GetOrderID());
        return AddOrder(order.ToOrderPointer(exisitingOrder->GetOrderType()));
    }

    std::size_t Size() const {return orders_.size();}

    OrderbookLevelInfos GetOrderInfos() const{
        LevelInfos bidInfos, askInfos;
        bidInfos.reserve(orders_.size());
        askInfos.reserve(orders_.size());

        auto CreateLevelInfos = [](Price price, const OrderPointers& orders){
            return LevelInfo{price, std::accumulate(orders.begin(, orders.end(), (Quantity)0, [](std::size_t runningSum, const OrderPointer& order) {return runningSum + order->GetRemainingQuantity();}))};
        };

        for(const auto& [price, orders] : bids_){
            bidInfos.push_back(CreateLevelInfos(price, orders));
        }

        for(const auto& [price, orders] : asks_){
            askInfos.push_back(CreateLevelInfos(price, orders));
        }

        return OrderBookLevelInfos{bidInfos, askInfos};
    }
};


