
#pragma once

#include <bit>
#include <cstdint>

#pragma pack(push)
#pragma pack(1)

static_assert(std::endian::native == std::endian::little); // this will only work in little endian system

struct XMPatternHeader
{
	uint32_t	header_size;
	uint8_t		packing; // always 0
	uint16_t	rows; // must be < 256
	uint16_t	packed_pattern_data_size;
};

static_assert(sizeof(XMPatternHeader) == 9);

#pragma pack(pop)
