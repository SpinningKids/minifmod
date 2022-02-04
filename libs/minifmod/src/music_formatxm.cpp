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

#define FMUSIC_XMLINEARPERIOD2HZ(_per) ( (int)(8363.0f*powf(2.0f, ((6.0f*12.0f*16.0f*4.0f - (_per)) / (float)(12*16*4)))) )

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
static void FMUSIC_XM_Portamento(FMUSIC_CHANNEL &channel) noexcept
{
	channel.period = std::clamp(channel.portatarget, channel.period - channel.portaspeed, channel.period + channel.portaspeed);
}
#endif // FMUSIC_XM_PORTATO_ACTIVE


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
static void FMUSIC_XM_Vibrato(FMUSIC_CHANNEL &channel) noexcept
{
	switch (channel.wavecontrol_vibrato)
	{
	    case WaveControl::Sine:
		{
			channel.freqdelta = -int(sinf((float)channel.tremolopos * (2 * 3.141592f / 64.0f)) * channel.tremolodepth) * 4;
			break;
		}
	    case WaveControl::SawTooth:
		{
			channel.freqdelta = (channel.vibpos + 4) * channel.vibdepth / 126 * 4;
			break;
		}
	    case WaveControl::Square:
	    case WaveControl::Random:
		{
			channel.freqdelta = (channel.tremolopos >= 0) ? - channel.tremolodepth * 8 : channel.tremolodepth * 8;							// square
			break;
		}
	}
}
#endif // defined(FMUSIC_XM_VIBRATO_ACTIVE) || defined(FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE)

#ifdef FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE
static void FMUSIC_XM_InstrumentVibrato(FMUSIC_CHANNEL &channel, const FMUSIC_INSTRUMENT *iptr) noexcept
{
	int delta;

	switch (iptr->sample_header.vibrato_type)
	{
	    case XMInstrumentVibratoType::Sine:
		{
		    delta = (int)(sinf( (float)channel.ivibpos * (2 * 3.141592f / 256.0f)) * 64.0f);
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
	delta >>=6;

	channel.freqdelta += delta;

	channel.ivibsweeppos = std::min(channel.ivibsweeppos + 1, int(iptr->sample_header.vibrato_sweep));
	channel.ivibpos = (channel.ivibpos + iptr->sample_header.vibrato_rate) & 255;
}
#endif	// FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE

#ifdef FMUSIC_XM_TREMOLO_ACTIVE
static void FMUSIC_XM_Tremolo(FMUSIC_CHANNEL &channel) noexcept
{
	switch(channel.wavecontrol_tremolo)
	{
	    case WaveControl::Sine:
		{
			channel.voldelta = int(sinf((float)channel.tremolopos * (2 * 3.141592f / 64.0f)) * (channel.tremolodepth*4));
			break;
		}
	    case WaveControl::SawTooth:
		{
			channel.voldelta = -(channel.tremolopos + 4) * channel.tremolodepth / 63;
			break;
		}
	    case WaveControl::Square:
		case WaveControl::Random:
		{
			channel.voldelta = (channel.tremolopos >= 0)? channel.tremolodepth*4:-channel.tremolodepth*4;							// square
			break;
		}
	}

	channel.tremolopos += channel.tremolospeed;
	if (channel.tremolopos > 31)
    {
		channel.tremolopos -=64;
    }
}
#endif // FMUSIC_XM_TREMOLO_ACTIVE

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
static void FMUSIC_XM_ProcessEnvelope(FMUSIC_CHANNEL &channel, EnvelopePoint *point, unsigned char type, const EnvelopePoints& envelope, unsigned char loopend, unsigned char loopstart, unsigned char sustain) noexcept
{
	if (envelope.count > 0)
    {
	    int currpos = 0;

		if ((type & XMEnvelopeFlagsLoop) && point->position == envelope.envelope[loopend].position)	// if we are at the correct tick for the position
		{
			// loop
			point->position = envelope.envelope[loopstart].position;
			currpos = loopstart;
		}
		else
		{
			// search new envelope position
			while (currpos < envelope.count - 1 && point->position > envelope.envelope[currpos + 1].position) currpos++;
		}

		// interpolate new envelope position

        point->value = envelope.envelope[currpos].value + envelope.envelope[currpos].delta * (point->position - envelope.envelope[currpos].position);

		// Envelope
		// if it is at the last position, abort the envelope and continue last value
        // same if we're at sustain point
		if (!(type & XMEnvelopeFlagsSustain) || currpos != sustain || channel.keyoff)
        {
			++point->position;
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
	if (volume >= 0x10 && volume <= 0x50)
	{
		channel.volume = volume-0x10;
	}
	else
	{
		switch (volume >> 4)
		{
			case 0x6:
			case 0x8:
			{
				channel.volume -= (volume & 0xF);
				break;
			}
			case 0x7:
			case 0x9:
			{
				channel.volume += (volume & 0xF);
				break;
			}
			case 0xa :
			{
				channel.vibspeed = (volume & 0xF);
				break;
			}
			case 0xb :
			{
				channel.vibdepth = (volume & 0xF);
				break;
			}
			case 0xc :
			{
				channel.pan = (volume & 0xF) << 4;
				break;
			}
			case 0xd :
			{
				channel.pan -= (volume & 0xF);
				break;
			}
			case 0xe :
			{
				channel.pan += (volume & 0xF);
				break;
			}
			case 0xf :
			{
				if (volume & 0xF)
                {
                    channel.portaspeed = (volume & 0xF) << 6;
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
static int FMUSIC_XM_GetAmigaPeriod(int note, int finetune) noexcept
{
    int period = (int)(powf(2.0f, (float)(132 - note) / 12.0f) * 13.375f);

	// interpolate for finer tuning
	if (finetune < 0 && note)
	{
		int diff = period - (int)(powf(2.0f, (float)(132-(note-1)) / 12.0f) * 13.375f);
		diff *= abs(finetune);
		diff /= 128;
		period -= diff;
	}
	else
	{
		int diff = (int)(powf(2.0f, (float)(132-(note+1)) / 12.0f) * 13.375f) - period;
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
static void FMUSIC_XM_UpdateFlags(FMUSIC_CHANNEL &channel, FSOUND_SAMPLE *sptr, FMUSIC_MODULE &mod) noexcept
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

        float high_precision_pan = std::clamp(channel.pan+ channel.envpan.value * (128 - abs(channel.pan - 128)), 0.0f, 255.0f); // 255

		constexpr float norm = 1.0f/ 68451041280.0f; // 2^27 (volume normalization) * 255.0 (pan scale) (*2 for safety?!?)

		float high_precision_volume = (channel.volume + channel.voldelta) * channel.fadeoutvol * mod.globalvolume * channel.envvol.value * norm;

		ccptr->leftvolume  = high_precision_volume * high_precision_pan;
		ccptr->rightvolume = high_precision_volume * (255 - high_precision_pan);
	}
	if ((channel.period + channel.freqdelta) != 0)
	{
		int freq = std::max(
			(mod.header.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY)
			    ? FMUSIC_XMLINEARPERIOD2HZ(channel.period + channel.freqdelta)
			    : FMUSIC_PERIOD2HZ(channel.period + channel.freqdelta),
			100l);

    	ccptr->speed = float(freq) / FSOUND_MixRate;
	}
	if (channel.stop)
	{
		ccptr->mixpos = 0;
//		ccptr->sptr = nullptr;
		ccptr->sampleoffset = 0;	// if this channel gets stolen it will be safe
	}
}

static void FMUSIC_XM_Resetcptr(FMUSIC_CHANNEL& channel, FSOUND_SAMPLE* sptr) noexcept
{
	channel.volume = (int)sptr->header.default_volume;
	channel.pan = sptr->header.default_panning;
	channel.envvol.value = 1.0;
	channel.envvolpos = 0;
	channel.envvol.position = 0;
	channel.envvol.delta = 0;

	channel.envpan.value = 0;
	channel.envpanpos = 0;
	channel.envpan.position = 0;
	channel.envpan.delta = 0;

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

static void XM_ProcessCommon(FMUSIC_CHANNEL& channel, FMUSIC_INSTRUMENT* iptr) noexcept
{
	//= PROCESS ENVELOPES ==========================================================================
#ifdef FMUSIC_XM_VOLUMEENVELOPE_ACTIVE
	FMUSIC_XM_ProcessEnvelope(channel, &channel.envvol, iptr->sample_header.volume_envelope_flags, iptr->volume_envelope, iptr->sample_header.volume_loop_end_index, iptr->sample_header.volume_loop_start_index, iptr->sample_header.volume_sustain_index);
#endif
#ifdef FMUSIC_XM_PANENVELOPE_ACTIVE
	FMUSIC_XM_ProcessEnvelope(channel, &channel.envpan, iptr->sample_header.pan_envelope_flags, iptr->pan_envelope, iptr->sample_header.pan_loop_end, iptr->sample_header.pan_loop_start, iptr->sample_header.pan_sustain_index);
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
        FMUSIC_INSTRUMENT		*iptr;
		FSOUND_SAMPLE			*sptr;

		const XMNote& note = row[count];
		FMUSIC_CHANNEL& channel = FMUSIC_Channel[count];

        unsigned char paramx = note.effect_parameter >> 4;			// get effect param x
		unsigned char paramy = note.effect_parameter & 0xF;			// get effect param y

//			**** FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1
//		if (LinkedListIsRootNode(cptr, &cptr.vchannelhead))
//			cptr = &FMUSIC_DummyVirtualChannel; // no channels allocated yet
//			**** FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1

		signed char porta = (note.effect == FMUSIC_XM_PORTATO || note.effect == FMUSIC_XM_PORTATOVOLSLIDE);

		// first store note and instrument number if there was one
		if (note.sample_index && !porta)							//  bugfix 3.20 (&& !porta)
        {
            channel.inst = note.sample_index-1;						// remember the Instrument #
        }

        if (note.note && note.note != FMUSIC_KEYOFF && !porta) //  bugfix 3.20 (&& !porta)
        {
            channel.note = note.note-1;						// remember the note
        }

        if (channel.inst >= (int)mod.header.instruments_count)
		{
			iptr = &FMUSIC_DummyInstrument;
			sptr = &FMUSIC_DummySample;
			sptr->buff = nullptr;
		}
		else
		{
			// set up some instrument and sample pointers
			iptr = &mod.instrument[channel.inst];
			if (iptr->sample_header.note_sample_number[channel.note] >= 16)
            {
				sptr = &FMUSIC_DummySample;
            }
			else
            {
				sptr = &iptr->sample[iptr->sample_header.note_sample_number[channel.note]];
            }

			if (!porta)
            {
				channel.sptr = sptr;
		    }
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
		channel.trigger = false;
		channel.stop = false;

		//= PROCESS NOTE ===============================================================================
		if (note.note && note.note != FMUSIC_KEYOFF)
		{
			// get note according to relative note
			channel.realnote = note.note + sptr->header.relative_note - 1;

			// get period according to realnote and finetune
			if (mod.header.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY)
			{
				channel.period_target = (10*12*16*4) - (channel.realnote*16*4) - (sptr->header.finetune / 2);
			}
			else
			{
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
				channel.period_target = FMUSIC_XM_GetAmigaPeriod(channel.realnote, sptr->header.finetune);
#endif
			}

			// frequency only changes if there are no portamento effects
			if (!porta)
            {
                channel.period = channel.period_target;
            }

            channel.trigger = true;
		}

		channel.freqdelta	= 0;

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
			channel.envvol.value = 0;
		}

		//= PROCESS TICK 0 EFFECTS =====================================================================
#if  1
		switch (note.effect)
		{
			// not processed on tick 0
#ifdef FMUSIC_XM_ARPEGGIO_ACTIVE
			case FMUSIC_XM_ARPEGGIO :
			{
				break;
			}
#endif
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
				if (note.effect_parameter)
                {
					channel.volslide = note.effect_parameter;
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
				if (note.effect_parameter)
                {
					channel.volslide = note.effect_parameter;
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
				if (note.effect_parameter)
                {
					channel.volslide  = note.effect_parameter;
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
					// not processed on tick 0 / unsupported
					case FMUSIC_XM_RETRIG :
					case FMUSIC_XM_NOTECUT :
					case FMUSIC_XM_SETFILTER :
					case FMUSIC_XM_FUNKREPEAT :
					case FMUSIC_XM_SETGLISSANDO :
					{
						break;
					}
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
					case FMUSIC_XM_SETFINETUNE :
					{
						sptr->header.finetune = paramy;
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
                            channel.finevslup = paramy;
                        }
                        channel.volume -= channel.finevslup;
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
				if (note.effect_parameter)
                {
					mod.globalvsl = note.effect_parameter;
                }
				break;
			}
#endif
#ifdef FMUSIC_XM_SETENVELOPEPOS_ACTIVE
			case FMUSIC_XM_SETENVELOPEPOS :
			{
				channel.envvol.position = note.effect_parameter;
				break;
			}
#endif
#ifdef FMUSIC_XM_PANSLIDE_ACTIVE
			case FMUSIC_XM_PANSLIDE :
			{
				if (note.effect_parameter)
				{
					channel.panslide = note.effect_parameter;
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

				if (paramx == 1)
				{
					if (paramy)
                    {
						channel.xtraportaup = paramy;
                    }
					channel.period -= channel.xtraportaup;
				}
				else if (paramx == 2)
				{
					if (paramy)
                    {
						channel.xtraportadown = paramy;
                    }
					channel.period += channel.xtraportadown;
				}
				break;
			}
#endif
		}
#endif
		XM_ProcessCommon(channel, iptr);

	    FMUSIC_XM_UpdateFlags(channel,sptr,mod);
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
        FMUSIC_INSTRUMENT		*iptr;
		FSOUND_SAMPLE			*sptr;
		const XMNote& note = row[count];
        FMUSIC_CHANNEL& channel = FMUSIC_Channel[count];

//			**** FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1
//		cptr = LinkedListNextNode(&cptr.vchannelhead);
//
//		if (LinkedListIsRootNode(cptr, &cptr.vchannelhead))
//			cptr = &FMUSIC_DummyVirtualChannel; // no channels allocated yet
//			**** FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1

		if (channel.inst >= (int)mod.header.instruments_count)
		{
			iptr = &FMUSIC_DummyInstrument;
			sptr = &FMUSIC_DummySample;
			sptr->buff = nullptr;
		}
		else
		{
			iptr = &mod.instrument[channel.inst];
			if (uint8_t note_sample = iptr->sample_header.note_sample_number[channel.note]; note_sample < 16)
            {
                sptr = &iptr->sample[note_sample];
            }
			else
            {
                sptr = &FMUSIC_DummySample;
            }
		}

		unsigned char effect = note.effect;			// grab the effect number
		unsigned char paramx = note.effect_parameter >> 4;		// grab the effect parameter x
		unsigned char paramy = note.effect_parameter & 0xF;		// grab the effect parameter y

		channel.voldelta	= 0;				// this is for tremolo / tremor etc
		channel.freqdelta	= 0;				// this is for vibrato / arpeggio etc
		channel.trigger = false;
		channel.stop = false;

		#ifdef FMUSIC_XM_VOLUMEBYTE_ACTIVE
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
				channel.volume -= (note.volume & 0xF);
				break;
			}
			case 0x7 :
			{
				channel.volume += (note.volume & 0xF);
				break;
			}
#ifdef FMUSIC_XM_VIBRATO_ACTIVE
			case 0xb :
			{
				channel.vibdepth = (note.volume & 0xF);

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
				channel.pan -= (note.volume & 0xF);
				break;
			}
			case 0xe :
			{
				channel.pan += (note.volume & 0xF);
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
                                channel.freqdelta = paramx << 6;
                            }
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
						    else
                            {
                                channel.freqdelta = FMUSIC_XM_GetAmigaPeriod(channel.realnote+paramx, sptr->header.finetune) -
                                    FMUSIC_XM_GetAmigaPeriod(channel.realnote, sptr->header.finetune);
                            }
#endif
						    break;
						}
						case 2:
						{
						    if (mod.header.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY)
                            {
                                channel.freqdelta = paramy << 6;
                            }
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
						    else
                            {
                                channel.freqdelta = FMUSIC_XM_GetAmigaPeriod(channel.realnote+paramy, sptr->header.finetune) -
                                    FMUSIC_XM_GetAmigaPeriod(channel.realnote, sptr->header.finetune);
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
				channel.freqdelta = 0;
				channel.period = std::max(channel.period - (channel.portaup << 2), 56); // subtract period and stop at B#8
				break;
			}
#endif
#ifdef FMUSIC_XM_PORTADOWN_ACTIVE
			case FMUSIC_XM_PORTADOWN :
			{
				channel.freqdelta = 0;
				channel.period += channel.portadown << 2; // subtract period
				break;
			}
#endif
#ifdef FMUSIC_XM_PORTATO_ACTIVE
			case FMUSIC_XM_PORTATO :
			{
				channel.freqdelta = 0;
				FMUSIC_XM_Portamento(channel);
				break;
			}
#endif
#ifdef FMUSIC_XM_VIBRATO_ACTIVE
			case FMUSIC_XM_VIBRATO :
			{
				FMUSIC_XM_Vibrato(channel);
				channel.vibpos += channel.vibspeed;
				if (channel.vibpos > 31)
                {
					channel.vibpos -= 64;
                }
				break;
			}
#endif
#ifdef FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE
			case FMUSIC_XM_PORTATOVOLSLIDE :
			{
				channel.freqdelta = 0;

				FMUSIC_XM_Portamento(channel);

				paramx = channel.volslide >> 4;    // grab the effect parameter x
				paramy = channel.volslide & 0xF;   // grab the effect parameter y

				// slide up takes precedence over down
				if (paramx)
				{
					channel.volume += paramx;
				}
				else
				{
					channel.volume -= paramy;
				}
				break;
			}
#endif
#ifdef FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE
			case FMUSIC_XM_VIBRATOVOLSLIDE :
			{
				FMUSIC_XM_Vibrato(channel);
				channel.vibpos += channel.vibspeed;
				if (channel.vibpos > 31)
                {
					channel.vibpos -= 64;
                }

				paramx = channel.volslide >> 4;    // grab the effect parameter x
				paramy = channel.volslide & 0xF;   // grab the effect parameter y

				// slide up takes precedence over down
				if (paramx)
				{
					channel.volume += paramx;
				}
				else
				{
					channel.volume -= paramy;
				}
				break;
			}
#endif
#ifdef FMUSIC_XM_TREMOLO_ACTIVE
			case FMUSIC_XM_TREMOLO :
			{
				FMUSIC_XM_Tremolo(channel);
				break;
			}
#endif
#ifdef FMUSIC_XM_VOLUMESLIDE_ACTIVE
			case FMUSIC_XM_VOLUMESLIDE :
			{
				paramx = channel.volslide >> 4;    // grab the effect parameter x
				paramy = channel.volslide & 0xF;   // grab the effect parameter y

				// slide up takes precedence over down
				if (paramx)
				{
					channel.volume += paramx;
				}
				else
				{
					channel.volume -= paramy;
				}
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
				if (!channel.retrigy)
                {
                    break; // divide by 0 bugfix
                }

				if (!(mod.tick % channel.retrigy))
				{
					if (channel.retrigx)
					{
						switch (channel.retrigx)
						{
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
					}
					channel.trigger = true;
				}
				break;
			}
#endif
#ifdef FMUSIC_XM_GLOBALVOLSLIDE_ACTIVE
			case FMUSIC_XM_GLOBALVOLSLIDE :
			{
				paramx = mod.globalvsl >> 4;    // grab the effect parameter x
				paramy = mod.globalvsl & 0xF;   // grab the effect parameter y

				// slide up takes precedence over down
				if (paramx)
				{
					mod.globalvolume = mod.globalvolume + paramx;
				}
				else
				{
					mod.globalvolume = mod.globalvolume - paramy;
				}
				break;
			}
#endif
#ifdef FMUSIC_XM_PANSLIDE_ACTIVE
			case FMUSIC_XM_PANSLIDE :
			{
				paramx = channel.panslide >> 4;    // grab the effect parameter x
				paramy = channel.panslide & 0xF;   // grab the effect parameter y

				// slide right takes precedence over left
				if (paramx)
				{
					channel.pan += paramx;
				}
				else
				{
					channel.pan -= paramy;
				}

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

			iptr->volume_envelope.count = (iptr->sample_header.volume_envelope_count < 2 || !(iptr->sample_header.volume_envelope_flags & XMEnvelopeFlagsOn)) ? 0 : iptr->sample_header.volume_envelope_count;
			iptr->pan_envelope.count = (iptr->sample_header.pan_envelope_count < 2 || !(iptr->sample_header.pan_envelope_flags & XMEnvelopeFlagsOn)) ? 0 : iptr->sample_header.pan_envelope_count;
			auto adjust_envelope = [](EnvelopePoints& e, const XMEnvelopePoint(&original_points)[12], int offset, float scale)
			{
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
			};
			adjust_envelope(iptr->volume_envelope, iptr->sample_header.volume_envelope, 0, 64);
			adjust_envelope(iptr->pan_envelope, iptr->sample_header.pan_envelope, 32, 32);

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
