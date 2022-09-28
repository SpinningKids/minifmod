
#pragma once

enum class WaveControl : uint8_t
{
	Sine,
	SawTooth,
	Square,
	Random,
};

class LFO final
{
	WaveControl wave_control_{};
	bool continue_{};
	int position_ = 0;
	int speed_ = 0;
	int depth_ = 0;
public:

	void setup(int speed, int depth)
	{
		if (speed) {
			speed_ = speed;
		}
		if (depth)
		{
			depth_ = depth * 4;
		}
	}

	void setFlags(uint8_t flags)
	{
		wave_control_ = (WaveControl)(flags & 3);
		continue_ = (flags & 4) != 0;
	}

	void reset()
	{
		if (!continue_)
		{
			position_ = 0;
		}
	}

	void update()
	{
		position_ += speed_;
		while (position_ > 31)
		{
			position_ -= 64;
		}
	}

	[[nodiscard]] int operator () () const
	{
		switch (wave_control_)
		{
		case WaveControl::Sine:
			return -int(sinf((float)position_ * (2 * 3.141592f / 64.0f)) * depth_);
		case WaveControl::SawTooth:
			return -(position_ * 2 + 1) * depth_ / 63;
		default:
			//case WaveControl::Square:
			//case WaveControl::Random:
			return (position_ >= 0) ? -depth_ : depth_; // square
		}
	}
};
