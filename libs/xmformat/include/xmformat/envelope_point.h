
#pragma once

#include <bit>
#include <cstdint>

#pragma pack(push)
#pragma pack(1)

static_assert(std::endian::native == std::endian::little); // this will only work in little endian system

struct XMEnvelopePoint
{
	uint16_t position;
	uint16_t value;
};

static_assert(sizeof(XMEnvelopePoint) == 4);

#pragma pack(pop)
