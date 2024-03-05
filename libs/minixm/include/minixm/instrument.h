
#pragma once

#include <cassert>

#include "envelope.h"
#include "sample.h"
#include "xmeffects.h"
#include <xmformat/instrument_header.h>
#include <xmformat/instrument_sample_header.h>
#include <xmformat/note.h>

// Multi sample extended instrument
struct Instrument final
{
	XMInstrumentHeader			header;
	XMInstrumentSampleHeader	instrument_sample_header;
#ifdef FMUSIC_XM_VOLUMEENVELOPE_ACTIVE
	EnvelopePoints				volume_envelope;
#endif
#ifdef FMUSIC_XM_PANENVELOPE_ACTIVE
	EnvelopePoints				pan_envelope;
#endif
	Sample						sample[16];		// 16 samples per instrument

	[[nodiscard]] const Sample& getSample(XMNote note) const
	{
		assert(note.isValid() && note.value < XMNote::KEY_OFF);
		const int note_sample = instrument_sample_header.note_sample_number[note.value - 1];
		assert(note_sample >= 0 && note_sample < 16);
		return sample[note_sample];
	}
};
