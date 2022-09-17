/******************************************************************************/
/* FORMATXM.C                                                                 */
/* ----------------                                                           */
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/

#include "music_formatxm.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <cmath>

#include <minifmod/minifmod.h>
#include "Music.h"
#include "Sound.h"
#include "system_file.h"
#include "xmeffects.h"

// Frequency = 8363*2^((6*12*16*4 - Period) / (12*16*4));

static inline int XMLinearPeriod2Frequency(int per)
{
	return (int)(8363.0f * powf(2.0f, (6.0f * 12.0f * 16.0f * 4.0f - per) / (float)(12 * 16 * 4)));
}

static inline int Period2Frequency(int period)
{
	return 14317456L / period;
}

#ifdef FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE
static void XMInstrumentVibrato(FMUSIC_CHANNEL& channel, const Instrument& instrument) noexcept
{
	int delta;

	switch (instrument.sample_header.vibrato_type)
	{
	    case XMInstrumentVibratoType::Sine:
	    {
		    delta = (int)(sinf((float)channel.ivibpos * (2 * 3.141592f / 256.0f)) * 64.0f);
		    break;
	    }
	    case XMInstrumentVibratoType::Square:
	    {
		    delta = 64 - (channel.ivibpos & 128);
		    break;
	    }
	    case XMInstrumentVibratoType::InverseSawTooth:
	    {
		    delta = (channel.ivibpos & 128) - ((channel.ivibpos + 1) / 4);
		    break;
	    }
	    case XMInstrumentVibratoType::SawTooth:
	    {
		    delta = ((channel.ivibpos + 1) / 4) - (channel.ivibpos & 128);
		    break;
	    }
	}

	delta *= instrument.sample_header.vibrato_depth;
	if (instrument.sample_header.vibrato_sweep)
	{
		delta = delta * channel.ivibsweeppos / instrument.sample_header.vibrato_sweep;
	}
	delta /= 64;

	channel.period_delta += delta;

	channel.ivibsweeppos = std::min(channel.ivibsweeppos + 1, int(instrument.sample_header.vibrato_sweep));
	channel.ivibpos = (channel.ivibpos + instrument.sample_header.vibrato_rate) & 255;
}
#endif	// FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE

/*
[API]
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
#if defined(FMUSIC_XM_VOLUMEENVELOPE_ACTIVE) || defined(FMUSIC_XM_PANENVELOPE_ACTIVE)
static void XMProcessEnvelope(int &position, float &value, const EnvelopePoints& envelope, XMEnvelopeFlags flags, unsigned char loop_start_index, unsigned char loop_end_index, unsigned char sustain_index, bool keyoff) noexcept
{
	if (envelope.count > 0)
    {

		if ((flags & XMEnvelopeFlagsLoop) && position == envelope.envelope[loop_end_index].position)	// if we are at the correct tick for the position
		{
			// loop
			position = envelope.envelope[loop_start_index].position;
		}
	    // search new envelope position
		uint8_t current_index = 0;
		while (current_index + 1 < envelope.count && position > envelope.envelope[current_index + 1].position) current_index++;

		// interpolate new envelope position

		value = envelope.envelope[current_index].value + envelope.envelope[current_index].delta * (position - envelope.envelope[current_index].position);

		// Envelope
		// if it is at the last position, abort the envelope and continue last value
        // same if we're at sustain point
		if (keyoff || current_index != sustain_index || !(flags & XMEnvelopeFlagsSustain))
        {
			++position;
		}
	}
}
#endif // (FMUSIC_XM_VOLUMEENVELOPE_ACTIVE) || defined(FMUSIC_XM_PANENVELOPE_ACTIVE)

/*
[API]
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
#ifdef FMUSIC_XM_VOLUMEBYTE_ACTIVE
static void FMUSIC_XM_ProcessVolumeByte(FMUSIC_CHANNEL &channel, unsigned char volume) noexcept
{
	// NOTE: Having this if makes code actually smaller than using the switch.
	if (volume >= 0x10 && volume <= 0x50)
	{
		channel.volume = volume-0x10;
	}
	else
	{
        int volumey = (volume & 0xF);
		switch (volume >> 4)
		{
			case 0x6:
			case 0x8:
			{
				channel.volume -= volumey;
				break;
			}
			case 0x7:
			case 0x9:
			{
				channel.volume += volumey;
				break;
			}
			case 0xa :
			{
				channel.vibrato.setup(volumey, 0); // TODO: Check if it's in the correct range (0-15)
				break;
			}
			case 0xb :
			{
				channel.vibrato.setup(0, volumey*2); // TODO: Check if it's in the correct range (0-15)
				break;
			}
			case 0xc :
			{
				channel.pan = volumey * 16;
				break;
			}
			case 0xd :
			{
				channel.pan -= volumey;
				break;
			}
			case 0xe :
			{
				channel.pan += volumey;
				break;
			}
			case 0xf :
			{
				channel.portamento.setup(channel.period_target, volumey * 64);
				channel.trigger = false;
				break;
			}
		}
	}
}
#endif // #define FMUSIC_XM_VOLUMEBYTE_ACTIVE

/*
[API]
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
static int GetAmigaPeriod(int note)
{
	return (int)(powf(2.0f, (float)(132 - note) / 12.0f) * 13.375f);
}

static int FMUSIC_XM_GetAmigaPeriod(int note, int8_t fine_tune) noexcept
{
	int period = GetAmigaPeriod(note);

	// interpolate for finer tuning
	int direction = (fine_tune > 0) ? 1 : -1;

    period += direction * ((GetAmigaPeriod(note + direction) - period) * fine_tune / 128);

	return period;
}
#endif // FMUSIC_XM_AMIGAPERIODS_ACTIVE

/*
[API]
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
#ifdef FMUSIC_XM_TREMOR_ACTIVE
static void FMUSIC_XM_Tremor(FMUSIC_CHANNEL &channel) noexcept
{
	if (channel.tremorpos >= channel.tremoron)
	{
	    channel.voldelta = -channel.volume;
	}
    channel.tremorpos++;
	if (channel.tremorpos >= (channel.tremoron + channel.tremoroff))
	{
	    channel.tremorpos = 0;
	}
}
#endif

/*
[API]
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
static void XMUpdateFlags(FMUSIC_CHANNEL &channel, const Sample &sample, FMUSIC_MODULE &mod) noexcept
{
    FSOUND_CHANNEL &sound_channel = *channel.cptr;

	if (channel.trigger)
	{
		// this swaps between channels to avoid sounds cutting each other off and causing a click
        if (sound_channel.sptr != nullptr)
        {
			int phaseout_sound_channel_number = sound_channel.index + 32;
			FSOUND_CHANNEL& phaseout_sound_channel = FSOUND_Channel[phaseout_sound_channel_number];
			phaseout_sound_channel = sound_channel;
			phaseout_sound_channel.index = phaseout_sound_channel_number;

            // this will cause the copy of the old channel to ramp out nicely.
			phaseout_sound_channel.leftvolume  = 0;
			phaseout_sound_channel.rightvolume = 0;
        }

        sound_channel.sptr = &sample;

		//==========================================================================================
		// START THE SOUND!
		//==========================================================================================
		if (sound_channel.sampleoffset >= sample.header.loop_start + sample.header.loop_length)
		{
		    sound_channel.sampleoffset = 0;
		}

		sound_channel.mixpos = (float)sound_channel.sampleoffset;
		sound_channel.speeddir  = MixDir::Forwards;
		sound_channel.sampleoffset = 0;	// reset it (in case other samples come in and get corrupted etc)

		// volume ramping
		sound_channel.filtered_leftvolume	= 0;
		sound_channel.filtered_rightvolume	= 0;
	}

	mod.globalvolume = std::clamp(mod.globalvolume, 0, 64);
	channel.volume = std::clamp(channel.volume, 0, 64);
	channel.voldelta = std::clamp(channel.voldelta, -channel.volume, 64 - channel.volume);
	channel.pan = std::clamp(channel.pan, 0, 255);

    float high_precision_pan = std::clamp(channel.pan+ channel.envpanvalue * (128 - abs(channel.pan - 128)), 0.0f, 255.0f); // 255

	constexpr float norm = 1.0f/ 68451041280.0f; // 2^27 (volume normalization) * 255.0 (pan scale) (*2 for safety?!?)

	float high_precision_volume = (channel.volume + channel.voldelta) * channel.fadeoutvol * mod.globalvolume * channel.envvolvalue * norm;

	sound_channel.leftvolume  = high_precision_volume * high_precision_pan;
	sound_channel.rightvolume = high_precision_volume * (255 - high_precision_pan);
	if ((channel.period + channel.period_delta) != 0)
	{
		int freq = std::max(
			(mod.header.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY)
			    ? XMLinearPeriod2Frequency(channel.period + channel.period_delta)
			    : Period2Frequency(channel.period + channel.period_delta),
			100);

    	sound_channel.speed = float(freq) / FSOUND_MixRate;
	}
	if (channel.stop)
	{
		sound_channel.mixpos = 0;
//		ccptr->sptr = nullptr;
		sound_channel.sampleoffset = 0;	// if this channel gets stolen it will be safe
	}
}

static void FMUSIC_XM_Resetcptr(FMUSIC_CHANNEL& channel, const Sample& sample) noexcept
{
	channel.volume = (int)sample.header.default_volume;
	channel.pan = sample.header.default_panning;
	channel.envvolpos = 0;
	channel.envvolvalue = 1.0;

	channel.envpanpos = 0;
	channel.envpanvalue = 0;

	channel.keyoff = false;
	channel.fadeoutvol = 32768;
	channel.ivibsweeppos = 0;
	channel.ivibpos = 0;

	// retrigger tremolo and vibrato waveforms
	channel.vibrato.reset();
	channel.tremolo.reset();

    channel.tremorpos = 0;								// retrigger tremor count
}

static void XMProcessCommon(FMUSIC_CHANNEL& channel, const Instrument& instrument) noexcept
{
	//= PROCESS ENVELOPES ==========================================================================
#ifdef FMUSIC_XM_VOLUMEENVELOPE_ACTIVE
	XMProcessEnvelope(channel.envvolpos, channel.envvolvalue, instrument.volume_envelope, instrument.sample_header.volume_envelope_flags, instrument.sample_header.volume_loop_start_index, instrument.sample_header.volume_loop_end_index, instrument.sample_header.volume_sustain_index, channel.keyoff);
#endif
#ifdef FMUSIC_XM_PANENVELOPE_ACTIVE
	XMProcessEnvelope(channel.envpanpos, channel.envpanvalue, instrument.pan_envelope, instrument.sample_header.pan_envelope_flags, instrument.sample_header.pan_loop_start_index, instrument.sample_header.pan_loop_end_index, instrument.sample_header.pan_sustain_index, channel.keyoff);
#endif
	//= PROCESS VOLUME FADEOUT =====================================================================
    if (channel.keyoff)
    {
        channel.fadeoutvol = std::max(channel.fadeoutvol - instrument.sample_header.volume_fadeout, 0);
    }
	//= INSTRUMENT VIBRATO ============================================================================
#ifdef FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE
	XMInstrumentVibrato(channel, instrument);	// this gets added to previous freqdeltas
#endif
}

/*
[API]
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
static void FMUSIC_UpdateXMNote(FMUSIC_MODULE& mod) noexcept
{
	bool pattern_jump = false;

    // process any rows commands to set the next order/row
	mod.order = mod.nextorder;
	mod.row = mod.nextrow;

	bool row_set = false;

	// Point our note pointer to the correct pattern buffer, and to the
	// correct offset in this buffer indicated by row and number of channels
	const auto& pattern = mod.pattern[mod.header.pattern_order[mod.order]];
	const auto& row = pattern[mod.row];

	// Loop through each channel in the row until we have finished
	for (int channel_index = 0; channel_index < mod.header.channels_count; channel_index++)
	{
		const XMNote& note = row[channel_index];
		FMUSIC_CHANNEL& channel = FMUSIC_Channel[channel_index];

		const unsigned char paramx = note.effect_parameter >> 4;			// get effect param x
		const unsigned char paramy = note.effect_parameter & 0xF;			// get effect param y
		const int slide = paramx ? paramx : -paramy;

		bool porta = (note.effect == FMUSIC_XM_PORTATO || note.effect == FMUSIC_XM_PORTATOVOLSLIDE);
		bool valid_note = note.note && note.note != FMUSIC_KEYOFF;

		if (!porta)
		{
			// first store note and instrument number if there was one
			if (note.sample_index)							//  bugfix 3.20 (&& !porta)
			{
				channel.inst = note.sample_index - 1;						// remember the Instrument #
			}

			if (valid_note) //  bugfix 3.20 (&& !porta)
			{
				channel.note = note.note - 1;						// remember the note
			}
		}

		assert(channel.inst < (int)mod.header.instruments_count);
		const Instrument& instrument = mod.instrument[channel.inst];

		uint8_t note_sample = instrument.sample_header.note_sample_number[channel.note];

		assert(note_sample < 16);
		const Sample& sample = instrument.sample[note_sample];

		if (valid_note) {
			channel.fine_tune = sample.header.fine_tune;
		}
		
		if (!porta)
		{
			channel.sptr = &sample;
		}

		int oldvolume = channel.volume;
		int oldperiod = channel.period;
		int oldpan = channel.pan;

		// if there is no more tremolo, set volume to volume + last tremolo delta
		if (channel.recenteffect == FMUSIC_XM_TREMOLO && note.effect != FMUSIC_XM_TREMOLO)
		{
			channel.volume += channel.voldelta;
		}
		channel.recenteffect = note.effect;

		channel.voldelta = 0;
		channel.trigger = valid_note;
		channel.period_delta = 0;				// this is for vibrato / arpeggio etc
		channel.stop = false;

		//= PROCESS NOTE ===============================================================================
		if (valid_note)
		{
			// get note according to relative note
			channel.realnote = note.note + sample.header.relative_note - 1;

			// get period according to realnote and fine_tune
			if (mod.header.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY)
			{
				channel.period_target = (10 * 12 * 16 * 4) - (channel.realnote * 16 * 4) - (channel.fine_tune / 2);
			}
			else
			{
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
				channel.period_target = FMUSIC_XM_GetAmigaPeriod(channel.realnote, channel.fine_tune);
#endif
			}

			// frequency only changes if there are no portamento effects
			if (!porta)
			{
				channel.period = channel.period_target;
			}
		}

		//= PROCESS INSTRUMENT NUMBER ==================================================================
		if (note.sample_index)
		{
			FMUSIC_XM_Resetcptr(channel, sample);
		}

		//= PROCESS VOLUME BYTE ========================================================================
#ifdef FMUSIC_XM_VOLUMEBYTE_ACTIVE
		if (note.volume)
		{
			FMUSIC_XM_ProcessVolumeByte(channel, note.volume);
		}
#endif

		//= PROCESS KEY OFF ============================================================================
		if (note.note == FMUSIC_KEYOFF || note.effect == FMUSIC_XM_KEYOFF)
		{
			channel.keyoff = true;
		}

		if (instrument.volume_envelope.count == 0 && channel.keyoff)
		{
			channel.envvolvalue = 0;
		}

		//= PROCESS TICK 0 EFFECTS =====================================================================
		switch (note.effect)
		{

#ifdef FMUSIC_XM_PORTAUP_ACTIVE
		case FMUSIC_XM_PORTAUP:
		{
			if (note.effect_parameter)
			{
				channel.portaup = note.effect_parameter;
			}
			break;
		}
#endif
#ifdef FMUSIC_XM_PORTADOWN_ACTIVE
		case FMUSIC_XM_PORTADOWN:
		{
			if (note.effect_parameter)
			{
				channel.portadown = note.effect_parameter;
			}
			break;
		}
#endif
#ifdef FMUSIC_XM_PORTATO_ACTIVE
		case FMUSIC_XM_PORTATO:
		{
			channel.portamento.setup(channel.period_target, note.effect_parameter * 4);
			channel.trigger = false;
			break;
		}
#endif
#ifdef FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE
		case FMUSIC_XM_PORTATOVOLSLIDE:
		{
			channel.portamento.setup(channel.period_target);
			if (slide)
			{
				channel.volslide = slide;
			}
			channel.trigger = false;
			break;
		}
#endif
#ifdef FMUSIC_XM_VIBRATO_ACTIVE
		case FMUSIC_XM_VIBRATO:
		{
			channel.vibrato.setup(paramx, paramy * 2); // TODO: Check if it's in the correct range (0-15)
			channel.period_delta = channel.vibrato();
			break;
		}
#endif
#ifdef FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE
		case FMUSIC_XM_VIBRATOVOLSLIDE:
		{
			if (slide)
			{
				channel.volslide = slide;
			}
			channel.period_delta = channel.vibrato();
			break;								// not processed on tick 0
		}
#endif
#ifdef FMUSIC_XM_TREMOLO_ACTIVE
		case FMUSIC_XM_TREMOLO:
		{
			channel.tremolo.setup(paramx, -paramy);
			break;
		}
#endif
#ifdef FMUSIC_XM_SETPANPOSITION_ACTIVE
		case FMUSIC_XM_SETPANPOSITION:
		{
			channel.pan = note.effect_parameter;
			break;
		}
#endif
#ifdef FMUSIC_XM_SETSAMPLEOFFSET_ACTIVE
		case FMUSIC_XM_SETSAMPLEOFFSET:
		{
			if (note.effect_parameter)
			{
				channel.sampleoffset = note.effect_parameter;
			}

			if (channel.cptr)
			{
				unsigned int offset = channel.sampleoffset * 256;

				if (offset >= sample.header.loop_start + sample.header.loop_length)
				{
					channel.trigger = false;
					channel.stop = true;
				}
				else
				{
					channel.cptr->sampleoffset = offset;
				}
			}
			break;
		}
#endif
#ifdef FMUSIC_XM_VOLUMESLIDE_ACTIVE
		case FMUSIC_XM_VOLUMESLIDE:
		{
			if (slide)
			{
				channel.volslide = slide;
			}
			break;
		}
#endif
#ifdef FMUSIC_XM_PATTERNJUMP_ACTIVE
		case FMUSIC_XM_PATTERNJUMP: // --- 00 B00 : --- 00 D63 , should put us at ord=0, row=63
		{
			mod.nextorder = note.effect_parameter % mod.header.song_length;
			mod.nextrow = 0;
			pattern_jump = true;
			row_set = true;
			break;
		}
#endif
#ifdef FMUSIC_XM_SETVOLUME_ACTIVE
		case FMUSIC_XM_SETVOLUME:
		{
			channel.volume = note.effect_parameter;
			break;
		}
#endif
#ifdef FMUSIC_XM_PATTERNBREAK_ACTIVE
		case FMUSIC_XM_PATTERNBREAK:
		{
			mod.nextrow = (paramx * 10) + paramy;
			if (mod.nextrow > 63) // NOTE: This seems odd, as the pattern might be longer than 64
			{
				mod.nextrow = 0;
			}
			if (!pattern_jump)
			{
				mod.nextorder = (mod.order + 1) % mod.header.song_length;
			}
			row_set = true;
			break;
		}
#endif
		// extended PT effects
		case FMUSIC_XM_SPECIAL:
		{
			switch (paramx)
			{
#ifdef FMUSIC_XM_FINEPORTAUP_ACTIVE
			case FMUSIC_XM_FINEPORTAUP:
			{
				if (paramy)
				{
					channel.fineportaup = paramy;
				}
				channel.period -= channel.fineportaup * 4;
				break;
			}
#endif
#ifdef FMUSIC_XM_FINEPORTADOWN_ACTIVE
			case FMUSIC_XM_FINEPORTADOWN:
			{
				if (paramy)
				{
					channel.fineportadown = paramy;
				}
				channel.period += channel.fineportadown * 4;
				break;
			}
#endif
#ifdef FMUSIC_XM_GLISSANDO_ACTIVE
			case FMUSIC_XM_GLISSANDO:
			{
				// not implemented
				break;
			}
#endif
#ifdef FMUSIC_XM_SETVIBRATOWAVE_ACTIVE
			case FMUSIC_XM_SETVIBRATOWAVE:
			{
				channel.vibrato.setFlags(paramy);
				break;
			}
#endif
#ifdef FMUSIC_XM_SETFINETUNE_ACTIVE
			case FMUSIC_XM_SETFINETUNE:
			{
				channel.fine_tune = paramy;
				break;
			}
#endif
#ifdef FMUSIC_XM_PATTERNLOOP_ACTIVE
			case FMUSIC_XM_PATTERNLOOP:
			{
				if (paramy == 0)
				{
					channel.patlooprow = mod.row;
				}
				else
				{
					if (channel.patloopno > 0)
					{
						channel.patloopno = paramy;
					}
					else
					{
						channel.patloopno--;
					}
					if (channel.patloopno)
					{
						mod.nextrow = channel.patlooprow;
						//mod.nextorder = mod.order; // This is not needed, as we initially set mod.order = mod.nextorder;
						row_set = true;
					}
				}
				break;
			}
#endif
#ifdef FMUSIC_XM_SETTREMOLOWAVE_ACTIVE
			case FMUSIC_XM_SETTREMOLOWAVE:
			{
				channel.tremolo.setFlags(paramy);
				break;
			}
#endif
#ifdef FMUSIC_XM_SETPANPOSITION16_ACTIVE
			case FMUSIC_XM_SETPANPOSITION16:
			{
				channel.pan = paramy * 16;
				break;
			}
#endif
#ifdef FMUSIC_XM_FINEVOLUMESLIDEUP_ACTIVE
			case FMUSIC_XM_FINEVOLUMESLIDEUP:
			{
				if (paramy)
				{
					channel.finevslup = paramy;
				}
				channel.volume += channel.finevslup;
				break;
			}
#endif
#ifdef FMUSIC_XM_FINEVOLUMESLIDEDOWN_ACTIVE
			case FMUSIC_XM_FINEVOLUMESLIDEDOWN:
			{
				if (paramy)
				{
					channel.finevsldown = paramy;
				}
				channel.volume -= channel.finevsldown;
				break;
			}
#endif
#ifdef FMUSIC_XM_NOTEDELAY_ACTIVE
			case FMUSIC_XM_NOTEDELAY:
			{
				channel.volume = oldvolume;
				channel.period = oldperiod;
				channel.pan = oldpan;
				channel.trigger = false;
				break;
			}
#endif
#ifdef FMUSIC_XM_PATTERNDELAY_ACTIVE
			case FMUSIC_XM_PATTERNDELAY:
			{
				mod.patterndelay = paramy;
				mod.patterndelay *= mod.speed;
				break;
			}
#endif
			}
			break;
		}
#ifdef FMUSIC_XM_SETSPED_ACTIVE
		case FMUSIEC_XM_SETSPEED:
		{
			if (note.effect_parameter < 0x20)
			{
				mod.speed = note.effect_parameter;
			}
			else
			{
				FMUSIC_SetBPM(mod, note.effect_parameter);
			}
			break;
		}
#endif
#ifdef FMUSIC_XM_SETGLOBALVOLUME_ACTIVE
		case FMUSIC_XM_SETGLOBALVOLUME:
		{
			mod.globalvolume = note.effect_parameter;
			break;
		}
#endif
#ifdef FMUSIC_XM_GLOBALVOLSLIDE_ACTIVE
		case FMUSIC_XM_GLOBALVOLSLIDE:
		{
			if (slide)
			{
				mod.globalvsl = slide;
			}
			break;
		}
#endif
#ifdef FMUSIC_XM_SETENVELOPEPOS_ACTIVE
		case FMUSIC_XM_SETENVELOPEPOS:
		{
			channel.envvolpos = note.effect_parameter;
			break;
		}
#endif
#ifdef FMUSIC_XM_PANSLIDE_ACTIVE
		case FMUSIC_XM_PANSLIDE:
		{
			if (slide)
			{
				channel.panslide = slide;
			}
			break;
		}
#endif
#ifdef FMUSIC_XM_MULTIRETRIG_ACTIVE
		case FMUSIC_XM_MULTIRETRIG:
		{
			if (note.effect_parameter)
			{
				channel.retrigx = paramx;
				channel.retrigy = paramy;
			}
			break;
		}
#endif
#ifdef FMUSIC_XM_TREMOR_ACTIVE
		case FMUSIC_XM_TREMOR:
		{
			if (note.effect_parameter)
			{
				channel.tremoron = (paramx + 1);
				channel.tremoroff = (paramy + 1);
			}
			FMUSIC_XM_Tremor(channel);
			break;
		}
#endif
#ifdef FMUSIC_XM_EXTRAFINEPORTA_ACTIVE
		case FMUSIC_XM_EXTRAFINEPORTA:
		{
			switch (paramx)
			{
			case 1:
			{
				if (paramy)
				{
					channel.xtraportaup = paramy;
				}
				channel.period -= channel.xtraportaup;
				break;
			}
			case 2:
			{
				if (paramy)
				{
					channel.xtraportadown = paramy;
				}
				channel.period += channel.xtraportadown;
				break;
			}
			}
			break;
		}
#endif
		}

		XMProcessCommon(channel, instrument);

		XMUpdateFlags(channel, sample, mod);

		// if there were no row commands
		if (!row_set)
		{
			if (mod.row + 1 < pattern.size())	// if end of pattern TODO: Fix signed/unsigned comparison
			{
                mod.nextrow = mod.row + 1;
            }
			else
			{
				mod.nextrow = 0;						// start at top of pattn
				if (mod.order + 1 < mod.header.song_length)
                {
                    mod.nextorder = mod.order + 1;
                }
                else
                {
                    mod.nextorder = mod.header.restart_position; // so increment the order
                }
            }
		}
	}
}

/*
[API]
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
static void FMUSIC_UpdateXMEffects(FMUSIC_MODULE& mod) noexcept
{
	// Point our note pointer to the correct pattern buffer, and to the
	// correct offset in this buffer indicated by row and number of channels
	const auto& pattern = mod.pattern[mod.header.pattern_order[mod.order]];
	const auto& row = pattern[mod.row];

	// Loop through each channel in the row until we have finished
	for (int channel_index = 0; channel_index < mod.header.channels_count; channel_index++)
	{
		const XMNote& note = row[channel_index];
		FMUSIC_CHANNEL& channel = FMUSIC_Channel[channel_index];

		const unsigned char paramx = note.effect_parameter >> 4;			// get effect param x
		const unsigned char paramy = note.effect_parameter & 0xF;			// get effect param y

		assert(channel.inst < (int)mod.header.instruments_count);
		const Instrument& instrument = mod.instrument[channel.inst];

		uint8_t note_sample = instrument.sample_header.note_sample_number[channel.note];

		assert(note_sample < 16);
		const Sample& sample = instrument.sample[note_sample];

		channel.voldelta = 0;
		channel.trigger = false;
		channel.period_delta = 0;				// this is for vibrato / arpeggio etc
		channel.stop = false;

#ifdef FMUSIC_XM_VOLUMEBYTE_ACTIVE
		int volumey = (note.volume & 0xF);
		switch (note.volume >> 4)
		{
			//			case 0x0:
			//			case 0x1:
			//			case 0x2:
			//			case 0x3:
			//			case 0x4:
			//			case 0x5:
			//				break;
		case 0x6:
		{
			channel.volume -= volumey;
			break;
		}
		case 0x7:
		{
			channel.volume += volumey;
			break;
		}
#ifdef FMUSIC_XM_VIBRATO_ACTIVE
		case 0xb:
		{
			channel.vibrato.setup(0, volumey * 2); // TODO: Check if it's in the correct range (0-15)
			channel.period_delta = channel.vibrato();
			channel.vibrato.update();
			break;
		}
#endif
		case 0xd:
		{
			channel.pan -= volumey;
			break;
		}
		case 0xe:
		{
			channel.pan += volumey;
			break;
		}
#ifdef FMUSIC_XM_PORTATO_ACTIVE
		case 0xf:
		{
			channel.period = channel.portamento(channel.period);
			break;
		}
#endif
		}
#endif


		//= PROCESS TICK N != 0 EFFECTS =====================================================================
		switch (note.effect)
		{
#ifdef FMUSIC_XM_ARPEGGIO_ACTIVE
		case FMUSIC_XM_ARPEGGIO:
		{
			int v;
			switch (mod.tick % 3)
			{
			case 0:
				v = 0;
				break;
			case 1:
				v = paramx;
				break;
			case 2:
				v = paramy;
				break;
			}
			if (mod.header.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY)
			{
				channel.period_delta = v * 64;
			}
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
			else
			{
				channel.period_delta = FMUSIC_XM_GetAmigaPeriod(channel.realnote + v, channel.fine_tune) -
					FMUSIC_XM_GetAmigaPeriod(channel.realnote, channel.fine_tune);
			}
#endif
			break;
		}
#endif
#ifdef FMUSIC_XM_PORTAUP_ACTIVE
		case FMUSIC_XM_PORTAUP:
		{
			channel.period_delta = 0;
			channel.period = std::max(channel.period - (channel.portaup * 4), 56); // subtract period and stop at B#8
			break;
		}
#endif
#ifdef FMUSIC_XM_PORTADOWN_ACTIVE
		case FMUSIC_XM_PORTADOWN:
		{
			channel.period_delta = 0;
			channel.period += channel.portadown * 4; // subtract period
			break;
		}
#endif
#if defined(FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_PORTATO_ACTIVE)
#ifdef FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE
		case FMUSIC_XM_PORTATOVOLSLIDE:
			channel.volume += channel.volslide;
#ifdef FMUSIC_XM_PORTATO_ACTIVE
			[[fallthrough]];
#endif
#endif
#ifdef FMUSIC_XM_PORTATO_ACTIVE
		case FMUSIC_XM_PORTATO:
#endif
			channel.period_delta = 0;
			channel.period = channel.portamento(channel.period);
			break;
#endif
#if defined (FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_VIBRATO_ACTIVE)
#ifdef FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE
		case FMUSIC_XM_VIBRATOVOLSLIDE:
			channel.volume += channel.volslide;
#ifdef FMUSIC_XM_VIBRATO_ACTIVE
			[[fallthrough]];
#endif
#endif
#ifdef FMUSIC_XM_VIBRATO_ACTIVE
		case FMUSIC_XM_VIBRATO:
#endif
			channel.period_delta = channel.vibrato();
			channel.vibrato.update();
			break;
#endif
#ifdef FMUSIC_XM_TREMOLO_ACTIVE
		case FMUSIC_XM_TREMOLO:
		{
			channel.voldelta = channel.tremolo();
			channel.tremolo.update();
			break;
		}
#endif
#ifdef FMUSIC_XM_VOLUMESLIDE_ACTIVE
		case FMUSIC_XM_VOLUMESLIDE:
		{
			channel.volume += channel.volslide;
			break;
		}
#endif
		// extended PT effects
		case FMUSIC_XM_SPECIAL:
		{
			switch (paramx)
			{
#ifdef FMUSIC_XM_RETRIG_ACTIVE
			case FMUSIC_XM_RETRIG:
			{
				if (paramy && mod.tick % paramy == 0)
				{
					channel.trigger = true;
				}
				break;
			}
#endif
#ifdef FMUSIC_XM_NOTECUT_ACTIVE
			case FMUSIC_XM_NOTECUT:
			{
				if (mod.tick == paramy)
				{
					channel.volume = 0;
				}
				break;
			}
#endif
#ifdef FMUSIC_XM_NOTEDELAY_ACTIVE
			case FMUSIC_XM_NOTEDELAY:
			{
				if (mod.tick == paramy)
				{
					//= PROCESS INSTRUMENT NUMBER ==================================================================
					FMUSIC_XM_Resetcptr(channel, sample);

					channel.period = channel.period_target;
#ifdef FMUSIC_XM_VOLUMEBYTE_ACTIVE
					if (note.volume)
					{
						FMUSIC_XM_ProcessVolumeByte(channel, note.volume);
					}
#endif
					channel.trigger = true;
				}
				break;
			}
#endif
			}
			break;
		}
#ifdef FMUSIC_XM_GLOBALVOLSLIDE_ACTIVE
		case FMUSIC_XM_GLOBALVOLSLIDE:
		{
			mod.globalvolume += mod.globalvsl;
			break;
		}
#endif
#ifdef FMUSIC_XM_PANSLIDE_ACTIVE
		case FMUSIC_XM_PANSLIDE:
		{
			channel.pan += channel.panslide;
			break;
		}
#endif
#ifdef FMUSIC_XM_MULTIRETRIG_ACTIVE
		case FMUSIC_XM_MULTIRETRIG:
		{
			if (channel.retrigy && !(mod.tick % channel.retrigy))
			{
				switch (channel.retrigx)
				{
				case 0:
				{
					break;
				}
				case 1:
				{
					channel.volume--;
					break;
				}
				case 2:
				{
					channel.volume -= 2;
					break;
				}
				case 3:
				{
					channel.volume -= 4;
					break;
				}
				case 4:
				{
					channel.volume -= 8;
					break;
				}
				case 5:
				{
					channel.volume -= 16;
					break;
				}
				case 6:
				{
					channel.volume = channel.volume * 2 / 3;
					break;
				}
				case 7:
				{
					channel.volume /= 2;
					break;
				}
				case 8:
				{
					// ?
					break;
				}
				case 9:
				{
					channel.volume++;
					break;
				}
				case 0xA:
				{
					channel.volume += 2;
					break;
				}
				case 0xB:
				{
					channel.volume += 4;
					break;
				}
				case 0xC:
				{
					channel.volume += 8;
					break;
				}
				case 0xD:
				{
					channel.volume += 16;
					break;
				}
				case 0xE:
				{
					channel.volume = channel.volume * 3 / 2;
					break;
				}
				case 0xF:
				{
					channel.volume *= 2;
					break;
				}
				}
				channel.trigger = true;
			}
			break;
		}
#endif
#ifdef FMUSIC_XM_TREMOR_ACTIVE
		case FMUSIC_XM_TREMOR:
		{
			FMUSIC_XM_Tremor(channel);
			break;
		}
#endif
		}

		XMProcessCommon(channel, instrument);

		XMUpdateFlags(channel, sample, mod);
	}
}

/*
[API]
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
void XMTick(FMUSIC_MODULE &mod) noexcept
{
	if (mod.tick == 0)									// new note
	{
		FMUSIC_UpdateXMNote(mod);					// Update and play the note
	}
	else
    {
		FMUSIC_UpdateXMEffects(mod);					// Else update the inbetween row effects
    }


	mod.tick++;
	if (mod.tick >= mod.speed + mod.patterndelay)
	{
		mod.patterndelay = 0;
		mod.tick = 0;
	}
}

/*
[API]
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
std::unique_ptr<FMUSIC_MODULE> XMLoad(void* fp, SAMPLELOADCALLBACK sampleloadcallback)
{
	// make sure DummySample is correctly initialized here.
	auto mod = std::make_unique<FMUSIC_MODULE>();

    FSOUND_File_Seek(fp, 0, SEEK_SET);
	FSOUND_File_Read(&mod->header, sizeof(mod->header), fp);

	// seek to patterndata
    FSOUND_File_Seek(fp, 60+mod->header.header_size, SEEK_SET);

	// unpack and read patterns
	for (uint16_t pattern_index=0; pattern_index < mod->header.patterns_count; pattern_index++)
	{
		XMPatternHeader header;
		FSOUND_File_Read(&header, sizeof(header), fp);

        Pattern& pattern = mod->pattern[pattern_index];
	    pattern.resize(header.rows);

		if (header.packed_pattern_data_size > 0)
		{
			for (uint16_t row = 0; row < header.rows; ++row)
			{
				auto &current_row = pattern[row];

				for (uint16_t channel_index = 0; channel_index < mod->header.channels_count; channel_index++)
				{
                    unsigned char dat;

					XMNote& note = current_row[channel_index];

					FSOUND_File_Read(&dat, 1, fp);
					if (dat & 0x80)
					{
						if (dat & 1)  FSOUND_File_Read(&note.note, 1, fp);
						if (dat & 2)  FSOUND_File_Read(&note.sample_index, 1, fp);
						if (dat & 4)  FSOUND_File_Read(&note.volume, 1, fp);
						if (dat & 8)  FSOUND_File_Read(&note.effect, 1, fp);
						if (dat & 16) FSOUND_File_Read(&note.effect_parameter, 1, fp);
					}
					else
					{
						note.note = dat;
						FSOUND_File_Read(((char*)&note) + 1, sizeof(XMNote) - 1, fp);
					}

					if (note.sample_index > 0x80)
					{
						note.sample_index = 0;
					}
				}
			}
		}
	}

	// load instrument information
	for (uint16_t instrument_index = 0; instrument_index < mod->header.instruments_count; ++instrument_index)
	{
        // point a pointer to that particular instrument
		Instrument& instrument = mod->instrument[instrument_index];

		int first_sample_offset = FSOUND_File_Tell(fp);
		FSOUND_File_Read(&instrument.header, sizeof(instrument.header), fp);				// instrument size
		first_sample_offset += instrument.header.header_size;

		assert(instrument.header.samples_count <= 16);

		if (instrument.header.samples_count > 0)
		{
			FSOUND_File_Read(&instrument.sample_header, sizeof(instrument.sample_header), fp);

			auto adjust_envelope = [](uint8_t count, const XMEnvelopePoint(&original_points)[12], int offset, float scale, XMEnvelopeFlags flags)
			{
				EnvelopePoints e;
				e.count = (count < 2 || !(flags & XMEnvelopeFlagsOn)) ? 0 : count;
				for (uint8_t i = 0; i < e.count; ++i)
				{
					e.envelope[i].position = original_points[i].position;
					e.envelope[i].value = (original_points[i].value - offset) / scale;
					if (i > 0)
					{
						const int tickdiff = e.envelope[i].position - e.envelope[i - 1].position;

						e.envelope[i - 1].delta = tickdiff ? (e.envelope[i].value - e.envelope[i - 1].value) / tickdiff : 0;
					}
				}
				if (e.count) {
					e.envelope[e.count - 1].delta = 0.f;
				}
				return e;
			};
			instrument.volume_envelope = adjust_envelope(instrument.sample_header.volume_envelope_count, instrument.sample_header.volume_envelope, 0, 64, instrument.sample_header.volume_envelope_flags);
			instrument.pan_envelope = adjust_envelope(instrument.sample_header.pan_envelope_count, instrument.sample_header.pan_envelope, 32, 32, instrument.sample_header.pan_envelope_flags);

			// seek to first sample
			FSOUND_File_Seek(fp, first_sample_offset, SEEK_SET);
			for (unsigned int sample_index = 0; sample_index < instrument.header.samples_count; sample_index++)
			{
				Sample& sample = instrument.sample[sample_index];

				FSOUND_File_Read(&sample.header, sizeof(sample.header), fp);

			    // type of sample
				if (sample.header.bits16)
				{
					sample.header.length /= 2;
					sample.header.loop_start /= 2;
					sample.header.loop_length /= 2;
				}

				if ((sample.header.loop_mode == XMLoopMode::Off) || (sample.header.length == 0))
				{
					sample.header.loop_start = 0;
					sample.header.loop_length = sample.header.length;
					sample.header.loop_mode = XMLoopMode::Off;
				}

			}

			// Load sample data
			for (unsigned int sample_index = 0; sample_index < instrument.header.samples_count; sample_index++)
			{
				Sample& sample = instrument.sample[sample_index];
				//unsigned int	samplelenbytes = sample.header.length * sample.bits / 8;

			    //= ALLOCATE MEMORY FOR THE SAMPLE BUFFER ==============================================

				if (sample.header.length)
				{
					sample.buff = std::make_unique<int16_t[]>(sample.header.length + 8);

					if (sampleloadcallback)
					{
						sampleloadcallback(sample.buff.get(), sample.header.length, instrument_index, sample_index);
						FSOUND_File_Seek(fp, sample.header.length*(sample.header.bits16?2:1), SEEK_CUR);
					}
					else
                    {
						if (sample.header.bits16)
						{
							FSOUND_File_Read(sample.buff.get(), sample.header.length*sizeof(short), fp);
						}
						else
						{
							int8_t *buff = new int8_t[sample.header.length + 8];
							FSOUND_File_Read(buff, sample.header.length, fp);
							for (uint32_t i = 0; i < sample.header.length; i++)
							{
								sample.buff[i] = static_cast<int16_t>(buff[i] * 256);
							}

							sample.header.bits16 = true;
							delete[] buff;
						}

						// DO DELTA CONVERSION
						int16_t previous_value = 0;
						for (uint32_t i = 0; i < sample.header.length; i++)
						{
							sample.buff[i] = previous_value = static_cast<int16_t>(sample.buff[i] + previous_value);
						}
					}

					// BUGFIX 1.3 - removed click for end of non looping sample (also size optimized a bit)
					if (sample.header.loop_mode == XMLoopMode::Bidi)
					{
						sample.buff[sample.header.loop_start+ sample.header.loop_length] = sample.buff[sample.header.loop_start+ sample.header.loop_length-1];// fix it
					}
					else if (sample.header.loop_mode == XMLoopMode::Normal)
					{
						sample.buff[sample.header.loop_start+ sample.header.loop_length] = sample.buff[sample.header.loop_start];// fix it
					}
				}
			}
		}
		else
		{
			FSOUND_File_Seek(fp, first_sample_offset, SEEK_SET);
		}
	}
	return mod;
}
