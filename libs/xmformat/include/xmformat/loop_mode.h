
#pragma once

#include <cstdint>

#pragma pack(push)
#pragma pack(1)

enum class XMLoopMode : uint8_t
{
	Off = 0,
	Normal = 1,		// For forward looping samples.
	Bidi = 2,		// For bidirectional looping samples.  (no effect if in hardware).
};

static_assert(sizeof(XMLoopMode) == 1);

#pragma pack(pop)
