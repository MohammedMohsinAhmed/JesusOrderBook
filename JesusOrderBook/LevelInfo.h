#pragma once

#include <vector>
#include "Ints.h"

struct LevelInfo {
	Price price_;
	Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;