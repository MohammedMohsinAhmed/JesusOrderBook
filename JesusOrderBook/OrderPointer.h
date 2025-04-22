#pragma once

#include <memory>
#include <list>

#include "Order.h"

// Because a single order can be in multiple data structs
// E.g in an order dict or bid/ask dict
using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;
