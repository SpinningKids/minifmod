
#pragma once

#include <algorithm>

class Portamento
{
	int target_;
	int speed_;
public:
	void setTarget(int target) { target_ = target; }
	void setSpeed(int speed) { if (speed) speed_ = speed; }

	int operator ()(int period) const noexcept
	{
		return std::clamp(target_, period - speed_, period + speed_);
	}
};