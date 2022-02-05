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

static inline int FMUSIC_XMLINEARPERIOD2HZ(int per)
{
	return (int)(8363.0f * powf(2.0f, (6.0f * 12.0f * 16.0f * 4.0f - per) / (float)(12 * 16 * 4)));
}

static inline int FMUSIC_PERIOD2HZ(int period)
{
	return 14317456L / period;
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
#if defined(FMUSIC_XM_PORTATO_ACTIVE) || defined(FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE)
static void FMUSIC_XM_Portamento(FMUSIC_CHANNEL& channel) noexcept
{
	channel.period = std::clamp(channel.portatarget, channel.period - channel.portaspeed, channel.period + channel.portaspeed);
}
#endif // FMUSIC_XM_PORTATO_ACTIVE

#if defined(FMUSIC_XM_VIBRATO_ACTIVE) || defined(FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_TREMOLO_ACTIVE)
int WaveControlFunction(WaveControl wave_control, int position, int depth)
{
	switch (wave_control)
	{
	    case WaveControl::Sine:
	    {
		    return -int(sinf((float)position * (2 * 3.141592f / 64.0f)) * depth * 4);
	    }
	    case WaveControl::SawTooth:
	    {
		    return -(position * 8 + 4) * depth / 63;
	    }
	    case WaveControl::Square:
	    case WaveControl::Random:
	    {
		    return (position >= 0) ? -depth * 4 : depth * 4;							// square
	    }
	}

}
#endif

/*
[API]
[
	[DESCRIPTION]
	to carry out a vibrato at a certain depth and speed

	[PARAMETERS]
	track - the track number to do the vibrato too

	[RETURN_VALUE]

	[REMARKS]
	AND'ing temp with 31 removes the sign bit giving the abs value

	[SEE_ALSO]
]
*/
#if defined(FMUSIC_XM_VIBRATO_ACTIVE) || defined(FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE)
static void FMUSIC_XM_Vibrato(FMUSIC_CHANNEL& channel) noexcept
{
	channel.period_delta = WaveControlFunction(channel.wavecontrol_vibrato, channel.vibpos, channel.vibdepth * 2);
}
#endif // defined(FMUSIC_XM_VIBRATO_ACTIVE) || defined(FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE)

#ifdef FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE
static void FMUSIC_XM_InstrumentVibrato(FMUSIC_CHANNEL& channel, const FMUSIC_INSTRUMENT* iptr) noexcept
{
	int delta;

	switch (iptr->sample_header.vibrato_type)
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
		    delta = (channel.ivibpos & 128) - ((channel.ivibpos + 1) >> 2);
		    break;
	    }
	    case XMInstrumentVibratoType::SawTooth:
	    {
		    delta = ((channel.ivibpos + 1) >> 2) - (channel.ivibpos & 128);
		    break;
	    }
	}

	delta *= iptr->sample_header.vibrato_depth;
	if (iptr->sample_header.vibrato_sweep)
	{
		delta = delta * channel.ivibsweeppos / iptr->sample_header.vibrato_sweep;
	}
	delta >>= 6;

	channel.period_delta += delta;

	channel.ivibsweeppos = std::min(channel.ivibsweeppos + 1, int(iptr->sample_header.vibrato_sweep));
	channel.ivibpos = (channel.ivibpos + iptr->sample_header.vibrato_rate) & 255;
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
static float FMUSIC_XM_ProcessEnvelope(int &position, unsigned char type, const EnvelopePoints& envelope, unsigned char loopend, unsigned char loopstart, unsigned char sustain, bool keyoff) noexcept
{
	if (envelope.count > 0)
    {
	    int current_index = 0;

		if ((type & XMEnvelopeFlagsLoop) && position == envelope.envelope[loopend].position)	// if we are at the correct tick for the position
		{
			// loop
			position = envelope.envelope[loopstart].position;
			current_index = loopstart;
		}
		else
		{
			// search new envelope position
			while (current_index < envelope.count - 1 && position > envelope.envelope[current_index + 1].position) current_index++;
		}

		// interpolate new envelope position

		const float value = envelope.envelope[current_index].value + envelope.envelope[current_index].delta * (position - envelope.envelope[current_index].position);

		// Envelope
		// if it is at the last position, abort the envelope and continue last value
        // same if we're at sustain point
		if (!(type & XMEnvelopeFlagsSustain) || current_index != sustain || keyoff)
        {
			++position;
		}
		return value;
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
				channel.vibspeed = volumey;
				break;
			}
			case 0xb :
			{
				channel.vibdepth = volumey;
				break;
			}
			case 0xc :
			{
				channel.pan = volumey << 4;
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
				if (volume & 0xF)
                {
                    channel.portaspeed = volumey << 6;
                }
                channel.portatarget = channel.period_target;
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

static int FMUSIC_XM_GetAmigaPeriod(int note, int finetune) noexcept
{
	int period = GetAmigaPeriod(note);

	// interpolate for finer tuning
	if (finetune < 0 && note)
	{
		int diff = period - GetAmigaPeriod(note - 1);
		diff *= abs(finetune);
		diff /= 128;
		period -= diff;
	}
	else
	{
		int diff = GetAmigaPeriod(note + 1) - period;
		diff *= finetune;
		diff /= 128;
		period += diff;
	}

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
static void FMUSIC_XM_UpdateFlags(FMUSIC_CHANNEL &channel, const FSOUND_SAMPLE *sptr, FMUSIC_MODULE &mod) noexcept
{
    FSOUND_CHANNEL *ccptr = channel.cptr;

    int channel_number = ccptr->index;

	if (channel.trigger)
	{
		//==========================================================================================
		// ALLOCATE A CHANNEL
		//==========================================================================================
		ccptr = &FSOUND_Channel[channel_number];

		// this swaps between channels to avoid sounds cutting each other off and causing a click
        if (ccptr->sptr != nullptr)
        {
			channel_number ^= 32;

            memcpy(&FSOUND_Channel[channel_number], ccptr, sizeof(FSOUND_CHANNEL));
            FSOUND_Channel[channel_number].index = channel_number; // oops dont want its index

            // this will cause the old channel to ramp out nicely.
		    ccptr->leftvolume  = 0;
		    ccptr->rightvolume = 0;

			ccptr = &FSOUND_Channel[channel_number];
			channel.cptr = ccptr;
		}

        ccptr->sptr = sptr;

		//==========================================================================================
		// START THE SOUND!
		//==========================================================================================
		if (ccptr->sampleoffset >= sptr->header.loop_start + sptr->header.loop_length)
		{
		    ccptr->sampleoffset = 0;
		}

		ccptr->mixpos  = ccptr->sampleoffset;
		ccptr->speeddir  = MixDir::Forwards;
		ccptr->sampleoffset = 0;	// reset it (in case other samples come in and get corrupted etc)

		// volume ramping
		ccptr->filtered_leftvolume	= 0;
		ccptr->filtered_rightvolume	= 0;
	}

	{
		mod.globalvolume = std::clamp(mod.globalvolume, 0, 64);
		channel.volume = std::clamp(channel.volume, 0, 64);
		channel.voldelta = std::clamp(channel.voldelta, -channel.volume, 64 - channel.volume);
		channel.pan = std::clamp(channel.pan, 0, 255);

        float high_precision_pan = std::clamp(channel.pan+ channel.envpanvalue * (128 - abs(channel.pan - 128)), 0.0f, 255.0f); // 255

		constexpr float norm = 1.0f/ 68451041280.0f; // 2^27 (volume normalization) * 255.0 (pan scale) (*2 for safety?!?)

		float high_precision_volume = (channel.volume + channel.voldelta) * channel.fadeoutvol * mod.globalvolume * channel.envvolvalue * norm;

		ccptr->leftvolume  = high_precision_volume * high_precision_pan;
		ccptr->rightvolume = high_precision_volume * (255 - high_precision_pan);
	}
	if ((channel.period + channel.period_delta) != 0)
	{
		int freq = std::max(
			(mod.header.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY)
			    ? FMUSIC_XMLINEARPERIOD2HZ(channel.period + channel.period_delta)
			    : FMUSIC_PERIOD2HZ(channel.period + channel.period_delta),
			100);

    	ccptr->speed = float(freq) / FSOUND_MixRate;
	}
	if (channel.stop)
	{
		ccptr->mixpos = 0;
//		ccptr->sptr = nullptr;
		ccptr->sampleoffset = 0;	// if this channel gets stolen it will be safe
	}
}

static void FMUSIC_XM_Resetcptr(FMUSIC_CHANNEL& channel, const FSOUND_SAMPLE* sptr) noexcept
{
	channel.volume = (int)sptr->header.default_volume;
	channel.pan = sptr->header.default_panning;
	channel.envvolpos = 0;
	channel.envvolvalue = 1.0;

	channel.envpanpos = 0;
	channel.envpanvalue = 0;

	channel.keyoff = false;
	channel.fadeoutvol = 32768;
	channel.ivibsweeppos = 0;
	channel.ivibpos = 0;

	// retrigger tremolo and vibrato waveforms
	if (!channel.continue_vibrato)
	{
		channel.vibpos = 0;
	}
	if (!channel.continue_tremolo)
	{
		channel.tremolopos = 0;
	}

	channel.tremorpos = 0;								// retrigger tremor count
}

static void XM_ProcessCommon(FMUSIC_CHANNEL& channel, const FMUSIC_INSTRUMENT* iptr) noexcept
{
	//= PROCESS ENVELOPES ==========================================================================
#ifdef FMUSIC_XM_VOLUMEENVELOPE_ACTIVE
	channel.envvolvalue = FMUSIC_XM_ProcessEnvelope(channel.envvolpos, iptr->sample_header.volume_envelope_flags, iptr->volume_envelope, iptr->sample_header.volume_loop_end_index, iptr->sample_header.volume_loop_start_index, iptr->sample_header.volume_sustain_index, channel.keyoff);
#endif
#ifdef FMUSIC_XM_PANENVELOPE_ACTIVE
	channel.envpanvalue = FMUSIC_XM_ProcessEnvelope(channel.envpanpos, iptr->sample_header.pan_envelope_flags, iptr->pan_envelope, iptr->sample_header.pan_loop_end_index, iptr->sample_header.pan_loop_start_index, iptr->sample_header.pan_sustain_index, channel.keyoff);
#endif
	//= PROCESS VOLUME FADEOUT =====================================================================
    if (channel.keyoff)
    {
        channel.fadeoutvol = std::max(channel.fadeoutvol - iptr->sample_header.volume_fadeout, 0);
    }
	//= INSTRUMENT VIBRATO ============================================================================
#ifdef FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE
	FMUSIC_XM_InstrumentVibrato(channel, iptr);	// this gets added to previous freqdeltas
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
static void FMUSIC_UpdateXMNote(FMUSIC_MODULE &mod) noexcept
{
    bool jumpflag = false;

    mod.nextorder = -1;
	mod.nextrow = -1;

	// Point our note pointer to the correct pattern buffer, and to the
	// correct offset in this buffer indicated by row and number of channels
	const auto &row = mod.pattern[mod.header.pattern_order[mod.order]].row(mod.row);

	// Loop through each channel in the row until we have finished
	for (int count = 0; count<mod.header.channels_count; count++)
	{
		const XMNote& note = row[count];
		FMUSIC_CHANNEL& channel = FMUSIC_Channel[count];

        const unsigned char paramx = note.effect_parameter >> 4;			// get effect param x
		const unsigned char paramy = note.effect_parameter & 0xF;			// get effect param y
		const int slide = paramx ? paramx : -paramy;

//			**** FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1
//		if (LinkedListIsRootNode(cptr, &cptr.vchannelhead))
//			cptr = &FMUSIC_DummyVirtualChannel; // no channels allocated yet
//			**** FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1

		bool porta = (note.effect == FMUSIC_XM_PORTATO || note.effect == FMUSIC_XM_PORTATOVOLSLIDE);

		// first store note and instrument number if there was one
		if (note.sample_index && !porta)							//  bugfix 3.20 (&& !porta)
        {
            channel.inst = note.sample_index-1;						// remember the Instrument #
        }

        bool valid_note = note.note && note.note != FMUSIC_KEYOFF;
        if (valid_note && !porta) //  bugfix 3.20 (&& !porta)
        {
            channel.note = note.note-1;						// remember the note
        }

		bool valid_instrument = channel.inst < (int)mod.header.instruments_count;
		const FMUSIC_INSTRUMENT* iptr = valid_instrument ? &mod.instrument[channel.inst] : &FMUSIC_DummyInstrument;

		uint8_t note_sample = valid_instrument ? iptr->sample_header.note_sample_number[channel.note] : 16;

		const FSOUND_SAMPLE* sptr = (note_sample < 16) ? &iptr->sample[note_sample] : &FMUSIC_DummySample;

		if (valid_note) {
			channel.finetune = sptr->header.finetune;
		}


		if (!porta)
		{
			channel.sptr = sptr;
		}

		int oldvolume = channel.volume;
		int oldperiod = channel.period;
		int oldpan = channel.pan;

		// if there is no more tremolo, set volume to volume + last tremolo delta
		if (channel.recenteffect == FMUSIC_XM_TREMOLO && note.effect != FMUSIC_XM_TREMOLO)
        {
			channel.volume += channel.voldelta;
        }
		channel.recenteffect  = note.effect;

		channel.voldelta = 0;
		channel.trigger = valid_note;
		channel.stop = false;

		//= PROCESS NOTE ===============================================================================
		if (valid_note)
		{
			// get note according to relative note
			channel.realnote = note.note + sptr->header.relative_note - 1;

			// get period according to realnote and finetune
			if (mod.header.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY)
			{
				channel.period_target = (10*12*16*4) - (channel.realnote*16*4) - (channel.finetune / 2);
			}
			else
			{
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
				channel.period_target = FMUSIC_XM_GetAmigaPeriod(channel.realnote, channel.finetune);
#endif
			}

			// frequency only changes if there are no portamento effects
			if (!porta)
            {
                channel.period = channel.period_target;
            }
		}

		channel.period_delta	= 0;

		//= PROCESS INSTRUMENT NUMBER ==================================================================
		if (note.sample_index)
        {
            FMUSIC_XM_Resetcptr(channel,sptr);
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

		if (iptr->volume_envelope.count == 0 && channel.keyoff)
		{
			channel.envvolvalue = 0;
		}

		//= PROCESS TICK 0 EFFECTS =====================================================================
		switch (note.effect)
		{
			// not processed on tick 0
#ifdef FMUSIC_XM_PORTAUP_ACTIVE
			case FMUSIC_XM_PORTAUP :
			{
				if (note.effect_parameter)
                {
					channel.portaup = note.effect_parameter;
                }
				break;
			}
#endif
#ifdef FMUSIC_XM_PORTADOWN_ACTIVE
			case FMUSIC_XM_PORTADOWN :
			{
				if (note.effect_parameter)
                {
					channel.portadown = note.effect_parameter;
                }
				break;
			}
#endif
#ifdef FMUSIC_XM_PORTATO_ACTIVE
			case FMUSIC_XM_PORTATO :
			{
				if (note.effect_parameter)
                {
					channel.portaspeed = note.effect_parameter << 2;
                }
				channel.portatarget = channel.period_target;
				channel.trigger = false;
				break;
			}
#endif
#ifdef FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE
			case FMUSIC_XM_PORTATOVOLSLIDE :
			{
				channel.portatarget = channel.period_target;
				if (slide)
                {
					channel.volslide = slide;
                }
				channel.trigger = false;
				break;
			}
#endif
#ifdef FMUSIC_XM_VIBRATO_ACTIVE
			case FMUSIC_XM_VIBRATO :
			{
				if (paramx)
                {
					channel.vibspeed = paramx;
                }
				if (paramy)
                {
					channel.vibdepth = paramy;
                }
				FMUSIC_XM_Vibrato(channel);
				break;
			}
#endif
#ifdef FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE
			case FMUSIC_XM_VIBRATOVOLSLIDE :
			{
				if (slide)
                {
					channel.volslide = slide;
                }
				FMUSIC_XM_Vibrato(channel);
				break;								// not processed on tick 0
			}
#endif
#ifdef FMUSIC_XM_TREMOLO_ACTIVE
			case FMUSIC_XM_TREMOLO :
			{
				if (paramx)
                {
					channel.tremolospeed = paramx;
                }
				if (paramy)
                {
					channel.tremolodepth = paramy;
                }
				break;
			}
#endif
#ifdef FMUSIC_XM_SETPANPOSITION_ACTIVE
			case FMUSIC_XM_SETPANPOSITION :
			{
				channel.pan = note.effect_parameter;
				break;
			}
#endif
#ifdef FMUSIC_XM_SETSAMPLEOFFSET_ACTIVE
			case FMUSIC_XM_SETSAMPLEOFFSET :
			{
                if (note.effect_parameter)
                {
					channel.sampleoffset = note.effect_parameter;
                }

				if (channel.cptr)
                {
                    unsigned int offset = (int)(channel.sampleoffset) << 8;

                    if (offset >= sptr->header.loop_start + sptr->header.loop_length)
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
			case FMUSIC_XM_VOLUMESLIDE :
			{
				if (slide)
                {
					channel.volslide  = slide;
                }
				break;
			}
#endif
#ifdef FMUSIC_XM_PATTERNJUMP_ACTIVE
			case FMUSIC_XM_PATTERNJUMP : // --- 00 B00 : --- 00 D63 , should put us at ord=0, row=63
			{
				mod.nextorder = note.effect_parameter;
				mod.nextrow = 0;
				if (mod.nextorder >= (int)mod.header.song_length)
                {
					mod.nextorder=0;
                }
				jumpflag = true;
				break;
			}
#endif
#ifdef FMUSIC_XM_SETVOLUME_ACTIVE
			case FMUSIC_XM_SETVOLUME :
			{
				channel.volume = note.effect_parameter;
				break;
			}
#endif
#ifdef FMUSIC_XM_PATTERNBREAK_ACTIVE
			case FMUSIC_XM_PATTERNBREAK :
			{
                mod.nextrow = (paramx*10) + paramy;
				if (mod.nextrow > 63)
                {
					mod.nextrow = 0;
                }
				if (!jumpflag)
                {
					mod.nextorder = mod.order+1;
                }
				if (mod.nextorder >= (int)mod.header.song_length)
                {
					mod.nextorder=0;
                }
				break;
			}
#endif
			case FMUSIC_XM_SPECIAL :
			{
				switch (paramx)
				{
#ifdef FMUSIC_XM_FINEPORTAUP_ACTIVE
					case FMUSIC_XM_FINEPORTAUP :
					{
						if (paramy)
                        {
							channel.fineportaup = paramy;
                        }
						channel.period -= (channel.fineportaup << 2);
						break;
					}
#endif
#ifdef FMUSIC_XM_FINEPORTADOWN_ACTIVE
					case FMUSIC_XM_FINEPORTADOWN :
					{
						if (paramy)
                        {
							channel.fineportadown = paramy;
                        }
						channel.period += (channel.fineportadown << 2);
						break;
					}
#endif
#ifdef FMUSIC_XM_SETVIBRATOWAVE_ACTIVE
					case FMUSIC_XM_SETVIBRATOWAVE :
					{
						channel.wavecontrol_vibrato = (WaveControl)(paramy & 3);
						channel.continue_vibrato = (paramy & 4) != 0;
						break;
					}
#endif
#ifdef FMUSIC_XM_SETFINETUNE_ACTIVE
					case FMUSIC_XM_SETFINETUNE:
					{
						channel.finetune += paramy;
						break;
					}
#endif
#ifdef FMUSIC_XM_PATTERNLOOP_ACTIVE
					case FMUSIC_XM_PATTERNLOOP :
					{
						if (paramy == 0)
                        {
							channel.patlooprow = mod.row;
                        }
						else
						{
							if (!channel.patloopno)
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
                            }
						}
						break;
					}
#endif
#ifdef FMUSIC_XM_SETTREMOLOWAVE_ACTIVE
					case FMUSIC_XM_SETTREMOLOWAVE :
					{
						channel.wavecontrol_tremolo = (WaveControl)(paramy & 3);
						channel.continue_tremolo = (paramy & 4) != 0;
						break;
					}
#endif
#ifdef FMUSIC_XM_SETPANPOSITION16_ACTIVE
					case FMUSIC_XM_SETPANPOSITION16 :
					{
						channel.pan = paramy<<4;
						break;
					}
#endif
#ifdef FMUSIC_XM_FINEVOLUMESLIDEUP_ACTIVE
					case FMUSIC_XM_FINEVOLUMESLIDEUP :
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
					case FMUSIC_XM_FINEVOLUMESLIDEDOWN :
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
					case FMUSIC_XM_NOTEDELAY :
					{
						channel.volume  = oldvolume;
						channel.period  = oldperiod;
						channel.pan     = oldpan;
						channel.trigger = false;
						break;
					}
#endif
#ifdef FMUSIC_XM_PATTERNDELAY_ACTIVE
					case FMUSIC_XM_PATTERNDELAY :
					{
						mod.patterndelay = paramy;
						mod.patterndelay *= mod.speed;
						break;
					}
#endif
				}
				break;
			}
#ifdef FMUSIC_XM_SETSPEED_ACTIVE
			case FMUSIC_XM_SETSPEED :
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
			case FMUSIC_XM_SETGLOBALVOLUME :
			{
				mod.globalvolume = note.effect_parameter;
				break;
			}
#endif
#ifdef FMUSIC_XM_GLOBALVOLSLIDE_ACTIVE
			case FMUSIC_XM_GLOBALVOLSLIDE :
			{
				if (slide)
                {
					mod.globalvsl = slide;
                }
				break;
			}
#endif
#ifdef FMUSIC_XM_SETENVELOPEPOS_ACTIVE
			case FMUSIC_XM_SETENVELOPEPOS :
			{
				channel.envvolpos = note.effect_parameter;
				break;
			}
#endif
#ifdef FMUSIC_XM_PANSLIDE_ACTIVE
			case FMUSIC_XM_PANSLIDE :
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
			case FMUSIC_XM_TREMOR :
			{
				if (note.effect_parameter)
				{
					channel.tremoron = (paramx+1);
					channel.tremoroff = (paramy+1);
				}
				FMUSIC_XM_Tremor(channel);
				break;
			}
#endif
#ifdef FMUSIC_XM_EXTRAFINEPORTA_ACTIVE
			case FMUSIC_XM_EXTRAFINEPORTA :
			{
				switch(paramx)
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
		XM_ProcessCommon(channel, iptr);

		FMUSIC_XM_UpdateFlags(channel, sptr, mod);
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
static void FMUSIC_UpdateXMEffects(FMUSIC_MODULE &mod) noexcept
{
    const auto& row = mod.pattern[mod.header.pattern_order[mod.order]].row(mod.row);

	for (int count = 0; count<mod.header.channels_count; count++)
	{
		const XMNote& note = row[count];
        FMUSIC_CHANNEL& channel = FMUSIC_Channel[count];

//			**** FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1
//		cptr = LinkedListNextNode(&cptr.vchannelhead);
//
//		if (LinkedListIsRootNode(cptr, &cptr.vchannelhead))
//			cptr = &FMUSIC_DummyVirtualChannel; // no channels allocated yet
//			**** FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1

		bool valid_instrument = channel.inst < (int)mod.header.instruments_count;
		const FMUSIC_INSTRUMENT* iptr = valid_instrument ? &mod.instrument[channel.inst] : &FMUSIC_DummyInstrument;

		uint8_t note_sample = valid_instrument ? iptr->sample_header.note_sample_number[channel.note] : 16; // samble must be invalid if instrument is invalid

		const FSOUND_SAMPLE* sptr = (note_sample < 16) ? &iptr->sample[note_sample] : &FMUSIC_DummySample;

		unsigned char effect = note.effect;			// grab the effect number
		const unsigned char paramx = note.effect_parameter >> 4;		// grab the effect parameter x
		const unsigned char paramy = note.effect_parameter & 0xF;		// grab the effect parameter y

		channel.voldelta	= 0;				// this is for tremolo / tremor etc
		channel.period_delta	= 0;				// this is for vibrato / arpeggio etc
		channel.trigger = false;
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
			case 0x7 :
			{
				channel.volume += volumey;
				break;
			}
#ifdef FMUSIC_XM_VIBRATO_ACTIVE
			case 0xb :
			{
				channel.vibdepth = volumey;

				FMUSIC_XM_Vibrato(channel);

				channel.vibpos += channel.vibspeed;
				if (channel.vibpos > 31)
                {
					channel.vibpos -= 64;
                }
				break;
			}
#endif
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
#ifdef FMUSIC_XM_PORTATO_ACTIVE
			case 0xf :
			{
				FMUSIC_XM_Portamento(channel);
				break;
			}
#endif
		}
		#endif


		switch(effect)
		{
#ifdef FMUSIC_XM_ARPEGGIO_ACTIVE
			case FMUSIC_XM_ARPEGGIO :
			{
				if (note.effect_parameter > 0)
				{
					switch (mod.tick % 3)
					{
						case 1:
						{
						    if (mod.header.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY)
                            {
                                channel.period_delta = paramx << 6;
                            }
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
						    else
                            {
                                channel.period_delta = FMUSIC_XM_GetAmigaPeriod(channel.realnote+paramx, channel.finetune) -
                                    FMUSIC_XM_GetAmigaPeriod(channel.realnote, channel.finetune);
                            }
#endif
						    break;
						}
						case 2:
						{
						    if (mod.header.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY)
                            {
                                channel.period_delta = paramy << 6;
                            }
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
						    else
                            {
                                channel.period_delta = FMUSIC_XM_GetAmigaPeriod(channel.realnote+paramy, channel.finetune) -
                                    FMUSIC_XM_GetAmigaPeriod(channel.realnote, channel.finetune);
                            }
#endif
						    break;
						}
					}
				}
				break;
			}
#endif
#ifdef FMUSIC_XM_PORTAUP_ACTIVE
			case FMUSIC_XM_PORTAUP :
			{
				channel.period_delta = 0;
				channel.period = std::max(channel.period - (channel.portaup << 2), 56); // subtract period and stop at B#8
				break;
			}
#endif
#ifdef FMUSIC_XM_PORTADOWN_ACTIVE
			case FMUSIC_XM_PORTADOWN :
			{
				channel.period_delta = 0;
				channel.period += channel.portadown << 2; // subtract period
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
			case FMUSIC_XM_PORTATO :
#endif
				channel.period_delta = 0;
				FMUSIC_XM_Portamento(channel);
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
    		    FMUSIC_XM_Vibrato(channel);
				channel.vibpos += channel.vibspeed;
				if (channel.vibpos > 31)
				{
					channel.vibpos -= 64;
				}
				break;
#endif
#ifdef FMUSIC_XM_TREMOLO_ACTIVE
			case FMUSIC_XM_TREMOLO :
			{
				channel.voldelta = WaveControlFunction(channel.wavecontrol_tremolo, channel.tremolopos, -channel.tremolodepth);
				channel.tremolopos += channel.tremolospeed;
				if (channel.tremolopos > 31)
				{
					channel.tremolopos -= 64;
				}
				break;
			}
#endif
#ifdef FMUSIC_XM_VOLUMESLIDE_ACTIVE
			case FMUSIC_XM_VOLUMESLIDE :
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
#ifdef FMUSIC_XM_NOTECUT_ACTIVE
					case FMUSIC_XM_NOTECUT:
					{
						if (mod.tick==paramy)
						{
							channel.volume = 0;
						}
						break;
					}
#endif
#ifdef FMUSIC_XM_RETRIG_ACTIVE
					case FMUSIC_XM_RETRIG :
					{
						if (paramy && mod.tick % paramy == 0)
                        {
							channel.trigger = true;
						}
                        break;
                    }
#endif
#ifdef FMUSIC_XM_NOTEDELAY_ACTIVE
					case FMUSIC_XM_NOTEDELAY :
					{
						if (mod.tick == paramy)
						{
							//= PROCESS INSTRUMENT NUMBER ==================================================================
							FMUSIC_XM_Resetcptr(channel,sptr);

							channel.period = channel.period_target;
#ifdef FMUSIC_XM_VOLUMEBYTE_ACTIVE
							if (note.volume)
                            {
								FMUSIC_XM_ProcessVolumeByte(channel, note.volume);
                            }
#endif
							channel.trigger = true;
						}
						else
						{
							channel.trigger = true;
						}
						break;
					}
#endif
				}
				break;
			}
#ifdef FMUSIC_XM_MULTIRETRIG_ACTIVE
			case FMUSIC_XM_MULTIRETRIG :
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
                            channel.volume >>= 1;
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
                            channel.volume <<= 1;
                            break;
                        }
                    }
                    channel.trigger = true;
                }
                break;
            }
#endif
#ifdef FMUSIC_XM_GLOBALVOLSLIDE_ACTIVE
			case FMUSIC_XM_GLOBALVOLSLIDE :
			{
				mod.globalvolume += mod.globalvsl;
				break;
			}
#endif
#ifdef FMUSIC_XM_PANSLIDE_ACTIVE
			case FMUSIC_XM_PANSLIDE :
			{
				channel.pan += channel.panslide;
    			break;
			}
#endif
#ifdef FMUSIC_XM_TREMOR_ACTIVE
			case FMUSIC_XM_TREMOR :
			{
				FMUSIC_XM_Tremor(channel);
				break;
			}
#endif
		}

		XM_ProcessCommon(channel, iptr);

		FMUSIC_XM_UpdateFlags(channel, sptr,mod);
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
void FMUSIC_UpdateXM(FMUSIC_MODULE &mod) noexcept
{
	if (mod.tick == 0)									// new note
	{
		// process any rows commands to set the next order/row
		if (mod.nextorder >= 0)
        {
			mod.order = mod.nextorder;
        }
		if (mod.nextrow >= 0)
        {
			mod.row = mod.nextrow;
        }

		FMUSIC_UpdateXMNote(mod);					// Update and play the note

		// if there were no row commands
		if (mod.nextrow == -1)
		{
			mod.nextrow = mod.row+1;
			if (mod.nextrow >= mod.pattern[mod.header.pattern_order[mod.order]].rows())	// if end of pattern
			{
				mod.nextorder = mod.order+1;			// so increment the order
				if (mod.nextorder >= (int)mod.header.song_length)
                {
					mod.nextorder = (int)mod.header.restart_position;
                }
				mod.nextrow = 0;						// start at top of pattn
			}
		}
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
std::unique_ptr<FMUSIC_MODULE> FMUSIC_LoadXM(void* fp, SAMPLELOADCALLBACK sampleloadcallback)
{
	// make sure DummySample is correctly initialized here.
	FMUSIC_DummySample.buff = nullptr;
	auto mod = std::make_unique<FMUSIC_MODULE>();

    FSOUND_File_Seek(fp, 0, SEEK_SET);
	FSOUND_File_Read(&mod->header, sizeof(mod->header), fp);

	// seek to patterndata
    FSOUND_File_Seek(fp, 60L+mod->header.header_size, SEEK_SET);

	// unpack and read patterns
	for (int count=0; count < mod->header.patterns_count; count++)
	{
		XMPatternHeader header;
		FSOUND_File_Read(&header, sizeof(header), fp);

	    mod->pattern[count].set_rows(header.rows);

		if (header.packed_pattern_data_size > 0)
		{
			for (int row = 0; row < header.rows; ++row)
			{
				auto &current_row = mod->pattern[count].row(row);

				for (int count2 = 0; count2 < mod->header.channels_count; count2++)
				{
                    unsigned char dat;

					XMNote* nptr = &current_row[count2];

					FSOUND_File_Read(&dat, 1, fp);
					if (dat & 0x80)
					{
						if (dat & 1)  FSOUND_File_Read(&nptr->note, 1, fp);
						if (dat & 2)  FSOUND_File_Read(&nptr->sample_index, 1, fp);
						if (dat & 4)  FSOUND_File_Read(&nptr->volume, 1, fp);
						if (dat & 8)  FSOUND_File_Read(&nptr->effect, 1, fp);
						if (dat & 16) FSOUND_File_Read(&nptr->effect_parameter, 1, fp);
					}
					else
					{
						nptr->note = dat;

						FSOUND_File_Read(((char*)nptr)+1, sizeof(XMNote) - 1, fp);

					}

					if (nptr->sample_index > 0x80)
					{
						nptr->sample_index = 0;
					}
				}
			}
		}
	}

	// load instrument information
	for (uint16_t count = 0; count < mod->header.instruments_count; ++count)
	{
        // point a pointer to that particular instrument
		FMUSIC_INSTRUMENT* iptr = &mod->instrument[count];

		int firstsampleoffset = FSOUND_File_Tell(fp);
		FSOUND_File_Read(&iptr->header, sizeof(iptr->header), fp);				// instrument size
		firstsampleoffset += iptr->header.header_size;

		assert(iptr->header.samples_count <= 16);

		if (iptr->header.samples_count > 0)
		{
			FSOUND_File_Read(&iptr->sample_header, sizeof(iptr->sample_header), fp);

			auto adjust_envelope = [](uint8_t count, const XMEnvelopePoint(&original_points)[12], int offset, float scale, uint8_t loop_start_index, uint8_t loop_end_index, uint8_t sustain_index, XMEnvelopeFlags flags)
			{
				EnvelopePoints e;
				e.count = (count < 2 || !(flags & XMEnvelopeFlagsOn)) ? 0 : count;
				for (int i = 0; i < e.count; ++i)
				{
					e.envelope[i].position = original_points[i].position;
					e.envelope[i].value = (original_points[i].value - offset) / scale;
					if (i > 0)
					{
						const int tickdiff = e.envelope[i].position - e.envelope[i - 1].position;

						e.envelope[i - 1].delta = tickdiff ? (e.envelope[i].value - e.envelope[i - 1].value) / tickdiff : 0;
					}
				}
				e.envelope[e.count - 1].delta = 0;
				return e;
			};
			iptr->volume_envelope = adjust_envelope(iptr->sample_header.volume_envelope_count, iptr->sample_header.volume_envelope, 0, 64, iptr->sample_header.volume_loop_start_index, iptr->sample_header.volume_loop_end_index, iptr->sample_header.volume_sustain_index, iptr->sample_header.volume_envelope_flags);
			iptr->pan_envelope = adjust_envelope(iptr->sample_header.pan_envelope_count, iptr->sample_header.pan_envelope, 32, 32, iptr->sample_header.pan_loop_start_index, iptr->sample_header.pan_loop_end_index, iptr->sample_header.pan_sustain_index, iptr->sample_header.pan_envelope_flags);

			// seek to first sample
			FSOUND_File_Seek(fp, firstsampleoffset, SEEK_SET);
			for (unsigned int count2 = 0; count2 < iptr->header.samples_count; count2++)
			{
				FSOUND_SAMPLE* sptr = &iptr->sample[count2];

				FSOUND_File_Read(&sptr->header, sizeof(sptr->header), fp);

			    // type of sample
				if (sptr->header.bits16)
				{
					sptr->header.length /= 2;
					sptr->header.loop_start /= 2;
					sptr->header.loop_length /= 2;
				}

				if ((sptr->header.loop_mode == XMLoopMode::Off) || (sptr->header.length == 0))
				{
					sptr->header.loop_start = 0;
					sptr->header.loop_length = sptr->header.length;
					sptr->header.loop_mode = XMLoopMode::Off;
				}

			}

			// Load sample data
			for (unsigned int count2 = 0; count2 < iptr->header.samples_count; count2++)
			{
				FSOUND_SAMPLE	*sptr = &iptr->sample[count2];
				//unsigned int	samplelenbytes = sptr->header.length * sptr->bits / 8;

			    //= ALLOCATE MEMORY FOR THE SAMPLE BUFFER ==============================================

				if (sptr->header.length)
				{
					sptr->buff = new short[sptr->header.length + 8];

					if (sampleloadcallback)
					{
						sampleloadcallback(sptr->buff, sptr->header.length, count, count2);
						FSOUND_File_Seek(fp, sptr->header.length*(sptr->header.bits16?2:1), SEEK_CUR);
					}
					else
                    {
						if (sptr->header.bits16)
						{
							FSOUND_File_Read(sptr->buff, sptr->header.length*sizeof(short), fp);
						}
						else
						{
							int8_t *buff = new int8_t[sptr->header.length + 8];
							FSOUND_File_Read(buff, sptr->header.length, fp);
							for (uint32_t count3 = 0; count3 < sptr->header.length; count3++)
							{
								sptr->buff[count3] = int16_t(buff[count3]) << 8;
							}

							sptr->header.bits16 = true;
							delete[] buff;
						}

						// DO DELTA CONVERSION
						int oldval = 0;
						for (uint32_t count3 = 0; count3 < sptr->header.length; count3++)
						{
                            sptr->buff[count3] = oldval = (short)(sptr->buff[count3] + oldval);
						}
					}

					// BUGFIX 1.3 - removed click for end of non looping sample (also size optimized a bit)
					if (sptr->header.loop_mode == XMLoopMode::Bidi)
					{
						sptr->buff[sptr->header.loop_start+sptr->header.loop_length] = sptr->buff[sptr->header.loop_start+sptr->header.loop_length-1];// fix it
					}
					else if (sptr->header.loop_mode == XMLoopMode::Normal)
					{
						sptr->buff[sptr->header.loop_start+sptr->header.loop_length] = sptr->buff[sptr->header.loop_start];// fix it
					}
				}
			}
		}
		else
		{
			FSOUND_File_Seek(fp, firstsampleoffset, SEEK_SET);
		}
	}
	return mod;
}
