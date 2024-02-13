
#pragma once

#include <bit>
#include <cstdint>

#include "loop_mode.h"

#pragma pack(push)
#pragma pack(1)

static_assert(std::endian::native == std::endian::little); // this will only work in little endian system

struct XMSampleHeader
{
	uint32_t	length;
	uint32_t	loop_start;
	uint32_t	loop_length;
	uint8_t		default_volume;
	int8_t		fine_tune;
	XMLoopMode	loop_mode : 2;
	uint8_t		skip_bits_1 : 2;
	bool		bits16 : 1;
	uint8_t		default_panning;
	int8_t		relative_note;
	uint8_t		reserved;
	char		sample_name[22];
};

static_assert(sizeof(XMSampleHeader) == 40);

#pragma pack(pop)
