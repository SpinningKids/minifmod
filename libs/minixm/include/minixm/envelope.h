
#pragma once

#include <xmformat/envelope_flags.h>

struct EnvelopePoint
{
	int position;
	float value;
	float delta;
};

struct EnvelopePoints
{
	int count;
	EnvelopePoint envelope[12];
};

class EnvelopeState
{
	int position_;
	float value_;
public:
	void reset(float value) noexcept
	{
		position_ = 0;
		value_ = value;
	}

	void process(const EnvelopePoints& envelope, XMEnvelopeFlags flags, unsigned char loop_start_index, unsigned char loop_end_index, unsigned char sustain_index, bool keyoff);
	float operator()() const noexcept { return value_; }
	void setPosition(int position) noexcept { position_ = position; }
};
