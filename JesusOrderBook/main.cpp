#include <iostream>
#include "Orderbook.h"

int main() {
	Orderbook orderbook;
	OrderId orderId = 1;
	OrderId orderIdTwo = 2;
	OrderId orderIdThree = 3;
	OrderId orderIdFour = 4;
	// TODO: Bug when attempting to fill a market order using orders at different price levels
	// E.g trying to fill a 200 bid with 100 and 120 ask
	orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, orderIdTwo, Side::Sell, 100, 50));
	orderbook.AddOrder(std::make_shared<Order>(orderId, Side::Buy, 100));
	std::cout << orderbook.Size() << '\n';
	orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, orderIdThree, Side::Sell, 120, 60));
	std::cout << orderbook.Size() << '\n';
	std::cin.get();
}




