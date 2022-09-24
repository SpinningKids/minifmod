
#pragma once

#include "envelope.h"
#include "sample.h"
#include "xmfile.h"

// Multi sample extended instrument
struct Instrument final
{
	XMInstrumentHeader			header;
	XMInstrumentSampleHeader	sample_header;
	EnvelopePoints				volume_envelope;
	EnvelopePoints				pan_envelope;
	Sample						sample[16];		// 16 samples per instrument
};
