
#pragma once

#include <bit>
#include <cstdint>

#pragma pack(push)
#pragma pack(1)

static_assert(std::endian::native == std::endian::little); // this will only work in little endian system

struct XMInstrumentHeader
{
	uint32_t	header_size;			// instrument size
	char		instrument_name[22];	// instrument filename
	uint8_t		instrument_type;		// instrument type (now 0)
	uint16_t	samples_count;			// number of samples in instrument
};

static_assert(sizeof(XMInstrumentHeader) == 29);

#pragma pack(pop)
