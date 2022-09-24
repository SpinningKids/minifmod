
#pragma once

#include <algorithm>

class Portamento
{
	int target_;
	int speed_;
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