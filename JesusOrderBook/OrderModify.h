#pragma once

#include "Ints.h"
#include "Order.h"
#include "Side.h"
#include "OrderPointer.h"


class OrderModify {
public:
	OrderModify(OrderId orderId, Side side, Price price, Quantity quantity)
		: orderId_{ orderId }
		, side_{ side }
		, price_{ price }
		, quantity_{ quantity }
	{
	}

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

