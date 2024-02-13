
#pragma once

#include <cstdint>

#pragma pack(push)
#pragma pack(1)

enum XMEnvelopeFlags : uint8_t
{
	XMEnvelopeFlagsOff = 0,
	XMEnvelopeFlagsOn = 1,
	XMEnvelopeFlagsSustain = 2,
	XMEnvelopeFlagsLoop = 4
};

static_assert(sizeof(XMEnvelopeFlags) == 1);

#pragma pack(pop)
