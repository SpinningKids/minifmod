
#pragma once

#include <algorithm>

class Portamento
{
	int target_ = 0;
	int speed_ = 0;
public:
	void setTarget(int target) { target_ = target; }
	void setSpeed(int speed) { if (speed) speed_ = speed; }

	int operator ()(int period) const noexcept
	{
		return std::clamp(target_, period - speed_, period + speed_);
	}
};