
#pragma once

#include <algorithm>

class Portamento
{
	float target_ = 0;
	float speed_ = 0;
public:
	void setTarget(float target) { target_ = target; }
	void setSpeed(float speed) { speed_ = speed; }

	float operator ()(float period) const noexcept
	{
		return std::clamp(target_, period - speed_, period + speed_);
	}
};