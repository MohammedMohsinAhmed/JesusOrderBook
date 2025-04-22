#pragma once

#include <map>
#include <unordered_map>

#include "OrderPointer.h"
#include "Trade.h"
#include "OrderModify.h"
#include "OrderBookLevelInfos.h"

class Orderbook {
private:
	struct OrderEntry {
		OrderPointer order_{ nullptr };
		OrderPointers::iterator location_;
	};

	std::map<Price, OrderPointers, std::greater<Price>> bids_;
	std::map<Price, OrderPointers, std::less<Price>> asks_;
	std::unordered_map<OrderId, OrderEntry> orders_;

	bool CanMatch(Side side, Price price) const;

	Trades MatchOrders();

public:
	Trades AddOrder(OrderPointer order);
	void CancelOrder(OrderId id);
	Trades MatchOrder(OrderModify order);
	std::size_t Size() const;
	OrderBookLevelInfos GetOrderInfos() const;
};