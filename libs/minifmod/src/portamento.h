
#pragma once

#include <algorithm>

class Portamento
{
	int target_ = 0;
	int speed_ = 0;
public:
	void setup(int target, int speed = 0)
	{
		if (speed) speed_ = speed;
		target_ = target;
	}

	int operator ()(int period) const noexcept
	{
		return std::clamp(target_, period - speed_, period + speed_);
	}
};