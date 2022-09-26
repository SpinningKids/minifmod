
#pragma once

#include <cstdint>
#include "xmeffects.h"
#include "xmfile.h"

struct EnvelopePoint
{
	int position;
	float value;
	float delta;
};

struct EnvelopePoints
{
	uint8_t count;
	EnvelopePoint envelope[12];
};

class EnvelopeState
{
	int position_;
	float value_;
public:
	void reset(float value)
	{
		position_ = 0;
		value_ = value;
	}

#if defined(FMUSIC_XM_VOLUMEENVELOPE_ACTIVE) || defined(FMUSIC_XM_PANENVELOPE_ACTIVE)
	void process(const EnvelopePoints& envelope, XMEnvelopeFlags flags, unsigned char loop_start_index, unsigned char loop_end_index, unsigned char sustain_index, bool keyoff) noexcept;
#endif
	float operator()() const { return value_; }
	void setPosition(int position) { position_ = position; }
};
