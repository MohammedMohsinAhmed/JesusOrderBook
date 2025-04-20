#include <cstdint>
#include <vector>
#include <format>
#include <stdexcept>
#include <memory>
#include <list>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <iostream>

enum class OrderType {
	GoodTillCancel,
	FillAndKill
};

enum class Side {
	Buy,
	Sell
};

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;

struct LevelInfo {
	Price price_;
	Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;

class OrderBookLevelInfos {
public:
	OrderBookLevelInfos(const LevelInfos& bids, const LevelInfos& asks)
		: bids_{ bids }
		, asks_{ asks }
	{ }

	const LevelInfos& GetBids() const { return bids_; };
	const LevelInfos& GetAsks() const { return asks_; };

private:
	LevelInfos bids_;
	LevelInfos asks_;
};

class Order {
public:
	Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity) 
		: orderType_{ orderType }
		, orderId_{ orderId }
		, side_{ side }
		, price_{ price }
		, initialQuantity_ { quantity }
		, remainingQuantity_ { quantity }
	{ }

	OrderId GetOrderId() const { return orderId_; }
	Side GetSide() const { return side_; }
	Price GetPrice() const { return price_; }
	OrderType GetOrderType() const { return orderType_; }
	Quantity GetInitialQuantity() const { return initialQuantity_; }
	Quantity GetRemainingQuantity() const { return remainingQuantity_; }
	Quantity GetFilledQuantity() const { return GetInitialQuantity() - GetRemainingQuantity(); }
	bool IsFilled() const { return GetRemainingQuantity() == 0; }
	void Fill(Quantity quantity) {
		if (quantity > GetRemainingQuantity()) 
			throw std::logic_error(std::format("Order ({}) cannot be filled for more than its remaining quantity.", GetOrderId()));

		remainingQuantity_ -= quantity;
	}

private:
	OrderType orderType_;
	OrderId orderId_;
	Side side_;
	Price price_;
	Quantity quantity_;
	Quantity initialQuantity_;
	Quantity remainingQuantity_;
};

// Because a single order can be in multiple data structs
// E.g in an order dict or bid/ask dict
using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

class OrderModify {
public:
	OrderModify(OrderId orderId, Side side, Price price, Quantity quantity)
		: orderId_{ orderId }
		, side_{ side }
		, price_{ price }
		, quantity_{ quantity }
	{}

	OrderId GetOrderId() const { return orderId_; }
	Side GetSide() const { return side_; }
	Price GetPrice() const { return price_; }
	Quantity GetQuantity() const { return quantity_; }

	OrderPointer ToOrderPointer(OrderType type) const {
		return std::make_shared<Order>(type, GetOrderId(), GetSide(), GetPrice(), GetQuantity());
	}

private:
	OrderId orderId_;
	Price price_;
	Side side_;
	Quantity quantity_;
};

struct TradeInfo {
	OrderId orderId_;
	Price price_;
	Quantity quantity_;
};

// Need representation for a matched order
class Trade {
public:
	Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade)
		: bidTrade_ { bidTrade }
		, askTrade_ { askTrade }
	{ }

	const TradeInfo& GetBidTrade() const { return bidTrade_; }
	const TradeInfo& GetAskTrade() const { return askTrade_; }

private:
	TradeInfo bidTrade_;
	TradeInfo askTrade_;
};

using Trades = std::vector<Trade>;

class Orderbook {
private: 
	struct OrderEntry {
		OrderPointer order_{ nullptr };
		OrderPointers::iterator location_;
	};

	std::map<Price, OrderPointers, std::greater<Price>> bids_;
	std::map<Price, OrderPointers, std::less<Price>> asks_;
	std::unordered_map<OrderId, OrderEntry> orders_;

	bool CanMatch(Side side, Price price) const {
		if (side == Side::Buy) {
			if (asks_.empty())
				return false;
			const auto& [bestAsk, _] = *asks_.begin();
			return price >= bestAsk;
		}
		else {
			if (bids_.empty()) 
				return false;
			const auto& [bestBid, _] = *bids_.begin();
			return price <= bestBid;
		}
	}

	Trades MatchOrders() {
		Trades trades;
		trades.reserve(orders_.size());

		while (true) {
			if (bids_.empty() || asks_.empty())
				break;

			auto& [bidPrice, bids] = *bids_.begin();
			auto& [askPrice, asks] = *asks_.begin();

			// The highest I'm willing to buy for is still lower than the lowest someome is willing to sell, no matching

			// In reality bid is usually lower than ask, creating the bid ask spread
			// Suppose highest bid was 50 and lowest ask was 50.5
			// A market maker might step in with a bid 50 and ask 50.4, attracing buyers who want to buy their shares at a discount to market ask
			// For each share sold, the market maker makes 0.4 

			if (bidPrice < askPrice)
				break;

			// todo: sort this size issue
			while (bids_.size() && asks_.size()) {
				// TODO: for some reason this code runs even when bids and asks are empty
				// After price level for bids and asks is removed, bids size becomes (very large number), same with asks
				// askPrice, asks,etc are all references to the private arrays. When we delete info on these private arrays, these references point to garbage
				auto bid = bids.front();
				auto ask = asks.front();

				Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());
				

				bid->Fill(quantity);
				ask->Fill(quantity);
				if (bid->IsFilled()) {
					bids.pop_front();
					orders_.erase(bid->GetOrderId());
				}

				if (ask->IsFilled()) {
					asks.pop_front();
					orders_.erase(ask->GetOrderId());
				}

				if (bids.empty()) {
					bids_.erase(bidPrice);
				}

				if (asks.empty()) {
					asks_.erase(askPrice);
				}

				Trade trade = Trade(TradeInfo(bid->GetOrderId(), bidPrice, quantity), TradeInfo(ask->GetOrderId(), askPrice, quantity));
				trades.push_back(trade);

			}

		}

		if (!bids_.empty()) {
			auto& [_, bids] = *bids_.begin();
			auto& order = bids.front();
			if (order->GetOrderType() == OrderType::FillAndKill) {
				CancelOrder(order->GetOrderId());
			}
		}

		if (!asks_.empty()) {
			auto& [_, asks] = *asks_.begin();
			auto& order = asks.front();
			if (order->GetOrderType() == OrderType::FillAndKill) {
				CancelOrder(order->GetOrderId());
			}
		}

		return trades;

	}

public:

	Trades AddOrder(OrderPointer order) {
		if (orders_.contains(order->GetOrderId()))
			return { };

		if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
			return { };

		OrderPointers::iterator iterator;
		
		if (order->GetSide() == Side::Buy) {
			// Create the price level if that price level doesnt exist already
			auto& orders = bids_[order->GetPrice()];
			orders.push_back(order);
			iterator = std::next(orders.begin(), orders.size() - 1);
		}
		else {
			auto& orders = asks_[order->GetPrice()];
			orders.push_back(order);
			iterator = std::next(orders.begin(), orders.size() - 1);
		}

		orders_.insert({ order->GetOrderId(), OrderEntry{order, iterator} });
		return MatchOrders();
	}

	void CancelOrder(OrderId id) {

		if (!orders_.contains(id)) {
			return;
		}

		const auto& [order, iterator] = orders_.at(id);

		Price price = order->GetPrice();
		if (order->GetSide() == Side::Buy) {
			OrderPointers& orders = bids_.at(price);
			orders.erase(iterator);
			if (orders.empty())
				bids_.erase(price);
		}
		else {
			OrderPointers& orders = asks_.at(price);
			orders.erase(iterator);
			if (orders.empty())
				asks_.erase(price);
		}

		orders_.erase(id);

	}

	Trades MatchOrder(OrderModify order) {
		 
		// To modify an order, simply cancel then create
		if (!orders_.contains(order.GetOrderId())) {
			return { };
		}

		const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
		// TODO: CancelOrder has a bug
		CancelOrder(order.GetOrderId());
		return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
	}

	std::size_t Size() const { return orders_.size(); }

	OrderBookLevelInfos GetOrderInfos() const {
		LevelInfos bidInfos, askInfos;
		bidInfos.reserve(orders_.size());
		askInfos.reserve(orders_.size());

		auto CreateLevelInfos = [](Price price, const OrderPointers& orders) {

			return LevelInfo{ price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
				[](Quantity runningSum, const OrderPointer& order)
				{ return runningSum + order->GetRemainingQuantity(); }) };

			};

		for (const auto& [price, orders] : bids_) {
			bidInfos.push_back(CreateLevelInfos(price, orders));
		}

		for (const auto& [price, orders] : asks_) {
			askInfos.push_back(CreateLevelInfos(price, orders));
		}

		return OrderBookLevelInfos{ bidInfos, askInfos };

	};

};

int main() {

	Orderbook orderbook;
	OrderId orderId = 1;
	OrderId orderIdTwo = 2;
	OrderId orderIdThree = 3;
	orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, orderIdThree, Side::Buy, 50, 10));
	orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, orderId, Side::Buy, 100, 10));
	orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, orderIdTwo, Side::Sell, 90, 1));
	orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 4, Side::Sell, 500, 1));
	orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 5, Side::Sell, 400, 1));
	std::cout << orderbook.Size() << '\n';
	OrderBookLevelInfos infos = orderbook.GetOrderInfos();
	std::cout << infos.GetBids()[0].price_ << '\n';
	std::cout << infos.GetBids()[0].quantity_ << '\n';
	std::cout << infos.GetAsks()[0].price_ << '\n';
	std::cout << infos.GetAsks()[0].quantity_ << '\n';
	orderbook.CancelOrder(orderId);
	std::cout << orderbook.Size() << '\n';
	std::cin.get();
}




