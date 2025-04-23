#include <numeric>
#include "Orderbook.h"


// For market orders, we can match as long as the 
// Total quantity of opposite side >= order quantity
// Though theres no need for this check
// Market orders can just stay in our orderbook
bool Orderbook::CanMatch(Side side, Price price) const {
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

// A market order doesnt care about price
// So bidprice < askPrice doesnt stop matching
Trades Orderbook::MatchOrders() {
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

		while (bids_.size() && asks_.size()) {
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

Trades Orderbook::AddOrder(OrderPointer order) {
	if (orders_.contains(order->GetOrderId()))
		return { };

	if (order->GetOrderType() == OrderType::Market) {
		if (order->GetSide() == Side::Buy && !asks_.empty()) {
			// A market order will fill at any price
			// Then, in the worst case scenario, the highest ask order we can trade with is going to be 
			// the worst ask
			const auto& [worstAsk, _] = *asks_.rbegin();
			order->ToGoodTillCancel(worstAsk);
		}
		else if (order->GetSide() == Side::Sell && !bids_.empty()) {
			// Same for bids
			const auto& [worstBid, _] = *bids_.rbegin();
			order->ToGoodTillCancel(worstBid);
		}
		else
			return { };
		// In this way we effectively create a limit order 
		// If the market ord quantity > opposite sides quantity
		// Then we fill the order when an order somes in on the other side 
		// Whos price can match with the new good till cancel
		// Which is basically a limit order 

	}

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

void Orderbook::CancelOrder(OrderId id) {

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

Trades Orderbook::MatchOrder(OrderModify order) {

	// To modify an order, simply cancel then create
	if (!orders_.contains(order.GetOrderId())) {
		return { };
	}

	const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
	CancelOrder(order.GetOrderId());
	return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
}

std::size_t Orderbook::Size() const { return orders_.size(); }

OrderBookLevelInfos Orderbook::GetOrderInfos() const {
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