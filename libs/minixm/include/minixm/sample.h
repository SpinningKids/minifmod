
#pragma once

#include <cstdint>
#include <memory>

#include <xmformat/sample_header.h>

// Sample type - contains info on sample
struct Sample final
{
	XMSampleHeader header;
	std::unique_ptr<int16_t[]> buff;			// pointer to sound data
};
