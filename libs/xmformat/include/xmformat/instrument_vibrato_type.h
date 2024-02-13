
#pragma once

#include <cstdint>

#pragma pack(push)
#pragma pack(1)

enum class XMInstrumentVibratoType : uint8_t
{
	Sine = 0,
	Square = 1,
	InverseSawTooth = 2,
	SawTooth = 3
};

static_assert(sizeof(XMInstrumentVibratoType) == 1);

#pragma pack(pop)
