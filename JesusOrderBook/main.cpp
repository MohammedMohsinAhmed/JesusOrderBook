#include <iostream>
#include "Orderbook.h"

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




