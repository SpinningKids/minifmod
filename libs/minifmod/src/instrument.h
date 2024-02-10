
#pragma once

#include <cassert>

#include "envelope.h"
#include "sample.h"
#include "xmeffects.h"
#include "xmfile.h"

// Multi sample extended instrument
struct Instrument final
{
	XMInstrumentHeader			header;
	XMInstrumentSampleHeader	sample_header;
#ifdef FMUSIC_XM_VOLUMEENVELOPE_ACTIVE
	EnvelopePoints				volume_envelope;
#endif
#ifdef FMUSIC_XM_PANENVELOPE_ACTIVE
	EnvelopePoints				pan_envelope;
#endif
	Sample						sample[16];		// 16 samples per instrument

	[[nodiscard]] const Sample& getSample(XMNote note) const noexcept
	{
		assert(note.isValid());
		const int note_sample = sample_header.note_sample_number[note.value - 1];
		assert(note_sample < 16);
		return sample[note_sample];
	}
};
