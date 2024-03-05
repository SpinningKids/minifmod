
#pragma once

#include "effect.h"
#include "note.h"

#pragma pack(push)
#pragma pack(1)

struct XMPatternCell
{
	XMNote note;
	uint8_t instrument_number;	// sample being played 0 = use last, 1-128 instrument number
	uint8_t volume;				// volume column value (0-64)  255=no volume
	XMEffect effect;			// effect number       (0-1Ah)
	uint8_t effect_parameter;	// effect parameter    (0-255)
};

static_assert(sizeof(XMPatternCell) == 5);

#pragma pack(pop)
