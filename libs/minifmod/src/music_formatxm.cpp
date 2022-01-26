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
#include "Mixer.h"
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
void FMUSIC_XM_Portamento(FMUSIC_CHANNEL &channel)
{
	// slide pitch down if it needs too.
	if (channel.freq < channel.portatarget)
	{
		channel.freq += (int)channel.portaspeed << 2;
		if (channel.freq > channel.portatarget)
        {
			channel.freq = channel.portatarget;
	    }
	}

	// slide pitch up if it needs too.
	else if (channel.freq > channel.portatarget)
	{
		channel.freq -= (int)channel.portaspeed << 2;
		if (channel.freq < channel.portatarget)
        {
			channel.freq=channel.portatarget;
	    }
	}

	 //
	 //if (glissando[track])
	 //{
	 //}
	 //

	 channel.notectrl |= FMUSIC_FREQ;
}
#endif // FMUSIC_XM_PORTATO_ACTIVE


#if defined(FMUSIC_XM_VIBRATO_ACTIVE) || defined(FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE)
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
void FMUSIC_XM_Vibrato(FMUSIC_CHANNEL &channel)
{
	int delta;

	switch (channel.wavecontrol & 3)
	{
		case 0:
		    {
		        delta = (int)fabsf(sinf( (float)channel.vibpos * (2 * 3.141592f / 64.0f)) * 256.0f);
		        break;
		    }
		case 1:
		    {
			    unsigned char temp = (channel.vibpos & 31) << 3;
		        if (channel.vibpos < 0)
		        {
		            temp=255-temp;
		        }
		        delta=temp;
		        break;
		    }
		case 2:
		case 3:
		    {
		        delta = 255;//rand()&255;					// random
		        break;
		    }
	}

	delta *= channel.vibdepth;
	delta >>=7;
	delta <<=2;   // we use 4*periods so make vibrato 4 times bigger

	if (channel.vibpos >= 0)
    {
		channel.freqdelta = -delta;
    }
	else
    {
		channel.freqdelta = delta;
    }

	channel.notectrl |= FMUSIC_FREQ;
}
#endif // defined(FMUSIC_XM_VIBRATO_ACTIVE) || defined(FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE)





#ifdef FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE

void FMUSIC_XM_InstrumentVibrato(FMUSIC_CHANNEL &channel, FMUSIC_INSTRUMENT *iptr)
{
	int delta;

	switch (iptr->sample_header.vibType)
	{
		case 0:
		    {
		        delta = (int)(sinf( (float)channel.ivibpos * (2 * 3.141592f / 256.0f)) * 64.0f);
		        break;
		    }
		case 1:
		    {
		        if (channel.ivibpos < 128)
		        {
		            delta=64;						// square
		        }
		        else
		        {
		            delta = -64;
		        }
		        break;
		    }
		case 2:
		    {
		        delta = (128-((channel.ivibpos+128)%256))>>1;
		        break;
		    }
		case 3:
		    {
		        delta = (128-(((256-channel.ivibpos)+128)%256))>>1;
		        break;
		    }
	}

	delta *= iptr->sample_header.vibDepth;
	if (iptr->sample_header.vibSweep)
    {
		delta = delta * channel.ivibsweeppos / iptr->sample_header.vibSweep;
    }
	delta >>=6;

	channel.freqdelta += delta;

	channel.ivibsweeppos++;
	if (channel.ivibsweeppos > iptr->sample_header.vibSweep)
    {
		channel.ivibsweeppos = iptr->sample_header.vibSweep;
    }

	channel.ivibpos += iptr->sample_header.vibRate;
	if (channel.ivibpos > 255)
    {
		channel.ivibpos -= 256;
    }

	channel.notectrl |= FMUSIC_FREQ;
}
#endif	// FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE


#ifdef FMUSIC_XM_TREMOLO_ACTIVE
void FMUSIC_XM_Tremolo(FMUSIC_CHANNEL &channel)
{
    unsigned char temp = (channel.tremolopos & 31);

	switch((channel.wavecontrol>>4) & 3)
	{
		case 0:
		    {
		        channel.voldelta = (int)fabsf(sinf( (float)channel.tremolopos * (2 * 3.141592f / 64.0f)) * 256.0f);
		        break;
		    }
		case 1:
		    {
		        temp <<= 3;										// ramp down
		        if (channel.tremolopos < 0)
		        {
		            temp=255-temp;
		        }
		        channel.voldelta=temp;
		        break;
		    }
		case 2:
		    {
		        channel.voldelta = 255;							// square
		        break;
		    }
		case 3:
		    {
		        channel.voldelta = (int)fabsf(sinf( (float)channel.tremolopos * (2 * 3.141592f / 64.0f)) * 256.0f);
		        break;
		    }
	}

	channel.voldelta *= channel.tremolodepth;
	channel.voldelta >>= 6;

	if (channel.tremolopos >= 0)
	{
		if (channel.volume+channel.voldelta > 64)
        {
			channel.voldelta = 64-channel.volume;
	    }
	}
	else
	{
		if ((short)(channel.volume-channel.voldelta) < 0)
        {
			channel.voldelta = channel.volume;
        }
		channel.voldelta = -channel.voldelta;
	}

	channel.tremolopos += channel.tremolospeed;
	if (channel.tremolopos > 31)
    {
		channel.tremolopos -=64;
    }

	channel.notectrl |= FMUSIC_VOLUME_PAN;
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

void FMUSIC_XM_ProcessEnvelope(FMUSIC_CHANNEL &channel, int *pos, int *tick, unsigned char type, int numpoints, unsigned short *points, unsigned char loopend, unsigned char loopstart, unsigned char sustain, int *value, int *valfrac, bool *envstopped, int *envdelta)
{
	// Envelope
	if (*pos < numpoints)
	{
		if (*tick == points[(*pos)<<1])	// if we are at the correct tick for the position
		{
            // handle loop
			if ((type & FMUSIC_ENVELOPE_LOOP) && *pos == loopend)
			{
				*pos  = loopstart;
				*tick = points[(*pos) <<1];
			}

			int currpos = *pos;
			int nextpos = currpos + 1;
			int currtick = points[currpos << 1];				// get tick at this point
			int nexttick = points[nextpos << 1];				// get tick at next point
			int curr = points[(currpos << 1) + 1] << 16;	// get  at this point << 16
			int next = points[(nextpos << 1) + 1] << 16;	// get  at next point << 16

			// if it is at the last position, abort the envelope and continue last value
			if (*pos == numpoints - 1)
			{
				*value = points[(currpos<<1)+1];
				*envstopped = true;
				channel.notectrl |= FMUSIC_VOLUME_PAN;
				return;
			}
			// sustain
			if ((type & FMUSIC_ENVELOPE_SUSTAIN) && currpos == sustain && !channel.keyoff)
			{
				*value = points[(currpos<<1)+1];
				channel.notectrl |= FMUSIC_VOLUME_PAN;
				return;
			}
			// interpolate 2 points to find delta step
			int tickdiff = nexttick - currtick;
			if (tickdiff)
				*envdelta = (next-curr) / tickdiff;
			else
				*envdelta = 0;

			*valfrac = curr;

			(*pos)++;
		}
		else
			*valfrac += *envdelta;				// interpolate
	}

	*value = *valfrac >> 16;
	(*tick)++;

	channel.notectrl |= FMUSIC_VOLUME_PAN;
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

void FMUSIC_XM_ProcessVolumeByte(FMUSIC_CHANNEL &channel, unsigned char volume)
{
	if (volume >= 0x10 && volume <= 0x50)
	{
		channel.volume = volume-0x10;
		channel.notectrl |= FMUSIC_VOLUME_PAN;
	}
	else
	{
		switch (volume >> 4)
		{
			case 0x6:
			case 0x8:
			{
				channel.volume -= (volume & 0xF);
				if (channel.volume < 0)
					channel.volume = 0;
				channel.notectrl |= FMUSIC_VOLUME_PAN;
				break;
			}
			case 0x7:
			case 0x9:
			{
				channel.volume += (volume & 0xF);
				if (channel.volume > 0x40)
					channel.volume = 0x40;
				channel.notectrl |= FMUSIC_VOLUME_PAN;
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
				channel.notectrl |= FMUSIC_VOLUME_PAN;
				break;
			}
			case 0xd :
			{
				channel.pan -= (volume & 0xF);
				channel.notectrl |= FMUSIC_VOLUME_PAN;
				break;
			}
			case 0xe :
			{
				channel.pan += (volume & 0xF);
				channel.notectrl |= FMUSIC_VOLUME_PAN;
				break;
			}
			case 0xf :
			{
				if (volume & 0xF)
					channel.portaspeed = (volume & 0xF) << 4;
				channel.portatarget = channel.period;
				channel.notectrl &= ~FMUSIC_TRIGGER;
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
int FMUSIC_XM_GetAmigaPeriod(int note, int finetune)
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
void FMUSIC_XM_Tremor(FMUSIC_CHANNEL &channel)
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
	channel.notectrl |= FMUSIC_VOLUME_PAN;
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
void FMUSIC_XM_UpdateFlags(FMUSIC_CHANNEL &channel, FSOUND_SAMPLE *sptr, FMUSIC_MODULE &mod)
{
    FSOUND_CHANNEL *ccptr = channel.cptr;

    int channel_number = ccptr->index;

	if (!(channel.freq + channel.freqdelta))
	{
		channel.notectrl &= ~FMUSIC_FREQ;	// divide by 0 check
	}

	if (channel.notectrl & FMUSIC_TRIGGER)
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

            // this should cause the old channel to ramp out nicely.
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
		ccptr->speeddir  = FSOUND_MIXDIR_FORWARDS;
		ccptr->sampleoffset = 0;	// reset it (in case other samples come in and get corrupted etc)

		// volume ramping
		ccptr->ramp_leftvolume	= 0;
		ccptr->ramp_rightvolume	= 0;
		ccptr->ramp_count		= 0;
	}

    if (channel.notectrl & FMUSIC_VOLUME_PAN)
	{
		int64_t high_precision_volume = int64_t(channel.envvol * (channel.volume + channel.voldelta) * channel.fadeoutvol) * mod.globalvolume;

        int high_precision_pan = std::clamp(channel.pan * 32 + (channel.envpan - 32) * (128 - abs(channel.pan - 128)), 0, 8191);

		constexpr float norm = 1.0f/281440616972288.0f; // 2^40/255.0

		float normalized_high_precision_volume = high_precision_volume * norm;

		ccptr->leftvolume  = normalized_high_precision_volume * high_precision_pan;
		ccptr->rightvolume = normalized_high_precision_volume * (8191 - high_precision_pan);

//		FSOUND_Software_SetVolume(&FSOUND_Channel[channel], (int)finalvol);
//		FSOUND_Software_SetPan(&FSOUND_Channel[channel], finalpan);
	}
	if (channel.notectrl & FMUSIC_FREQ)
	{
		int freq;

		if (mod.header.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY)
		{
		    freq = FMUSIC_XMLINEARPERIOD2HZ(channel.freq+channel.freqdelta);
		}
		else
		{
		    freq = FMUSIC_PERIOD2HZ(channel.freq+channel.freqdelta);
		}

		freq = std::max(freq, 100);

    	ccptr->speed = float(freq) / FSOUND_MixRate;
	}
	if (channel.notectrl & FMUSIC_STOP)
	{
//		FSOUND_StopSound(channel);

		ccptr->mixpos = 0;
//		ccptr->sptr = nullptr;
		ccptr->sampleoffset = 0;	// if this channel gets stolen it will be safe
	}
}



void FMUSIC_XM_Resetcptr(FMUSIC_CHANNEL &cptr, FSOUND_SAMPLE	*sptr)
{
	cptr.volume		= (int)sptr->header.default_volume;
	cptr.pan			= sptr->header.default_panning;
	cptr.envvol		= 64;
	cptr.envvolpos		= 0;
	cptr.envvoltick	= 0;
	cptr.envvoldelta	= 0;

	cptr.envpan		= 32;
	cptr.envpanpos		= 0;
	cptr.envpantick	= 0;
	cptr.envpandelta	= 0;

	cptr.keyoff		= false;
	cptr.fadeoutvol	= 65536;
	cptr.envvolstopped = false;
	cptr.envpanstopped = false;
	cptr.ivibsweeppos = 0;
	cptr.ivibpos = 0;

	// retrigger tremolo and vibrato waveforms
	if ((cptr.wavecontrol & 0xF) < 4)
    {
		cptr.vibpos=0;
    }
	if ((cptr.wavecontrol >> 4) < 4)
    {
		cptr.tremolopos=0;
    }

	cptr.tremorpos	= 0;								// retrigger tremor count

	cptr.notectrl |= FMUSIC_VOLUME_PAN;
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
void FMUSIC_UpdateXMNote(FMUSIC_MODULE &mod)
{
    signed char	jumpflag= false;

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

		const FMUSIC_NOTE& note = row[count];
		FMUSIC_CHANNEL& channel = FMUSIC_Channel[count];

        unsigned char paramx = note.eparam >> 4;			// get effect param x
		unsigned char paramy = note.eparam & 0xF;			// get effect param y

//			**** FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1
//		if (LinkedListIsRootNode(cptr, &cptr.vchannelhead))
//			cptr = &FMUSIC_DummyVirtualChannel; // no channels allocated yet
//			**** FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1

		signed char porta = (note.effect == FMUSIC_XM_PORTATO || note.effect == FMUSIC_XM_PORTATOVOLSLIDE);

		// first store note and instrument number if there was one
		if (note.number && !porta)							//  bugfix 3.20 (&& !porta)
			channel.inst = note.number-1;						// remember the Instrument #

		if (note.note && note.note != FMUSIC_KEYOFF && !porta) //  bugfix 3.20 (&& !porta)
			channel.note = note.note-1;						// remember the note

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
			if (iptr->sample_header.noteSmpNums[channel.note] >= 16)
            {
				sptr = &FMUSIC_DummySample;
            }
			else
            {
				sptr = &iptr->sample[iptr->sample_header.noteSmpNums[channel.note]];
            }

			if (!porta)
            {
				channel.sptr = sptr;
		    }
		}

		int oldvolume = channel.volume;
		int oldfreq = channel.freq;
		int oldpan = channel.pan;

		// if there is no more tremolo, set volume to volume + last tremolo delta
		if (channel.recenteffect == FMUSIC_XM_TREMOLO && note.effect != FMUSIC_XM_TREMOLO)
        {
			channel.volume += channel.voldelta;
        }
		channel.recenteffect  = note.effect;

		channel.voldelta = 0;
		channel.notectrl	= 0;

		//= PROCESS NOTE ===============================================================================
		if (note.note && note.note != FMUSIC_KEYOFF)
		{
			// get note according to relative note
			channel.realnote = note.note + sptr->header.relative_note - 1;

			// get period according to realnote and finetune
			if (mod.header.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY)
			{
				channel.period = (10*12*16*4) - (channel.realnote*16*4) - (sptr->header.finetune / 2);
			}
			else
			{
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
				channel.period = FMUSIC_XM_GetAmigaPeriod(channel.realnote, sptr->header.finetune);
#endif
			}

			// frequency only changes if there are no portamento effects
			if (!porta)
				channel.freq = channel.period;

			channel.notectrl = FMUSIC_TRIGGER;
		}

		channel.freqdelta	= 0;
		channel.notectrl		|= FMUSIC_FREQ;
		channel.notectrl		|= FMUSIC_VOLUME_PAN;


		//= PROCESS INSTRUMENT NUMBER ==================================================================
		if (note.number)
			FMUSIC_XM_Resetcptr(channel,sptr);

		//= PROCESS VOLUME BYTE ========================================================================
#ifdef FMUSIC_XM_VOLUMEBYTE_ACTIVE
		if (note.volume)
			FMUSIC_XM_ProcessVolumeByte(channel, note.volume);
#endif

		//= PROCESS KEY OFF ============================================================================
		if (note.note == FMUSIC_KEYOFF || note.effect == FMUSIC_XM_KEYOFF)
			channel.keyoff = true;

		//= PROCESS ENVELOPES ==========================================================================
#ifdef FMUSIC_XM_VOLUMEENVELOPE_ACTIVE
		if (iptr->sample_header.volEnvFlags & FMUSIC_ENVELOPE_ON)
		{
			if (!channel.envvolstopped)
				FMUSIC_XM_ProcessEnvelope(channel, &channel.envvolpos, &channel.envvoltick, iptr->sample_header.volEnvFlags, iptr->sample_header.numVolPoints, &iptr->sample_header.volEnvelope[0], iptr->sample_header.volLoopEnd, iptr->sample_header.volLoopStart, iptr->sample_header.volSustain, &channel.envvol, &channel.envvolfrac, &channel.envvolstopped, &channel.envvoldelta);
		}
		else
#endif
			if (channel.keyoff)
				channel.envvol = 0;

#ifdef FMUSIC_XM_PANENVELOPE_ACTIVE
		if (iptr->sample_header.panEnvFlags & FMUSIC_ENVELOPE_ON && !channel.envpanstopped)
			FMUSIC_XM_ProcessEnvelope(channel, &channel.envpanpos, &channel.envpantick, iptr->sample_header.panEnvFlags, iptr->sample_header.numPanPoints, &iptr->sample_header.panEnvelope[0], iptr->sample_header.panLoopEnd, iptr->sample_header.panLoopStart, iptr->sample_header.panSustain, &channel.envpan, &channel.envpanfrac, &channel.envpanstopped, &channel.envpandelta);
#endif

		//= PROCESS VOLUME FADEOUT =====================================================================
		if (channel.keyoff)
		{
			channel.fadeoutvol -= iptr->sample_header.volFadeout;
			if (channel.fadeoutvol < 0)
				channel.fadeoutvol = 0;
			channel.notectrl |= FMUSIC_VOLUME_PAN;
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
				if (note.eparam)
                {
					channel.portaup = note.eparam;
                }
				break;
			}
#endif
#ifdef FMUSIC_XM_PORTADOWN_ACTIVE
			case FMUSIC_XM_PORTADOWN :
			{
				if (note.eparam)
                {
					channel.portadown = note.eparam;
                }
				break;
			}
#endif
#ifdef FMUSIC_XM_PORTATO_ACTIVE
			case FMUSIC_XM_PORTATO :
			{
				if (note.eparam)
                {
					channel.portaspeed = note.eparam;
                }
				channel.portatarget = channel.period;
				channel.notectrl &= ~(FMUSIC_TRIGGER | FMUSIC_FREQ);
				break;
			}
#endif
#ifdef FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE
			case FMUSIC_XM_PORTATOVOLSLIDE :
			{
				channel.portatarget = channel.period;
				if (note.eparam)
                {
					channel.volslide = note.eparam;
                }
				channel.notectrl &= ~(FMUSIC_TRIGGER | FMUSIC_FREQ);
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
				if (note.eparam)
                {
					channel.volslide = note.eparam;
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
				channel.pan = note.eparam;
				channel.notectrl |= FMUSIC_VOLUME_PAN;
				break;
			}
#endif
#ifdef FMUSIC_XM_SETSAMPLEOFFSET_ACTIVE
			case FMUSIC_XM_SETSAMPLEOFFSET :
			{
                if (note.eparam)
                {
					channel.sampleoffset = note.eparam;
                }

				if (!channel.cptr)
                {
                    break;
                }

				unsigned int offset = (int)(channel.sampleoffset) << 8;

				if (offset >= sptr->header.loop_start + sptr->header.loop_length)
				{
					channel.notectrl &= ~FMUSIC_TRIGGER;
					channel.notectrl |= FMUSIC_STOP;
				}
				else
                {
					channel.sampleoffset = offset;
                }
				break;
			}
#endif
#ifdef FMUSIC_XM_VOLUMESLIDE_ACTIVE
			case FMUSIC_XM_VOLUMESLIDE :
			{
				if (note.eparam)
                {
					channel.volslide  = note.eparam;
                }
				break;
			}
#endif
#ifdef FMUSIC_XM_PATTERNJUMP_ACTIVE
			case FMUSIC_XM_PATTERNJUMP : // --- 00 B00 : --- 00 D63 , should put us at ord=0, row=63
			{
				mod.nextorder = note.eparam;
				mod.nextrow = 0;
				if (mod.nextorder >= (int)mod.header.song_length)
                {
					mod.nextorder=0;
                }
				jumpflag = 1;
				break;
			}
#endif
#ifdef FMUSIC_XM_SETVOLUME_ACTIVE
			case FMUSIC_XM_SETVOLUME :
			{
				channel.volume = note.eparam;
				channel.notectrl |= FMUSIC_VOLUME_PAN;
				break;
			}
#endif
#ifdef FMUSIC_XM_PATTERNBREAK_ACTIVE
			case FMUSIC_XM_PATTERNBREAK :
			{
                signed char breakflag=false;
                mod.nextrow = (paramx*10) + paramy;
				if (mod.nextrow > 63)
                {
					mod.nextrow = 0;
                }
				if (!breakflag && !jumpflag)
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
						channel.freq -= (channel.fineportaup << 2);
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
						channel.freq += (channel.fineportadown << 2);
						break;
					}
#endif
#ifdef FMUSIC_XM_SETVIBRATOWAVE_ACTIVE
					case FMUSIC_XM_SETVIBRATOWAVE :
					{
						channel.wavecontrol &= 0xF0;
						channel.wavecontrol |= paramy;
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
						channel.wavecontrol &= 0xF;
						channel.wavecontrol |= (paramy<<4);
						break;
					}
#endif
#ifdef FMUSIC_XM_SETPANPOSITION16_ACTIVE
					case FMUSIC_XM_SETPANPOSITION16 :
					{
						channel.pan = paramy<<4;
						channel.notectrl |= FMUSIC_VOLUME_PAN;
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

						if (channel.volume > 64)
                        {
							channel.volume=64;
                        }

						channel.notectrl |= FMUSIC_VOLUME_PAN;
						break;
					}
#endif
#ifdef FMUSIC_XM_FINEVOLUMESLIDEDOWN_ACTIVE
					case FMUSIC_XM_FINEVOLUMESLIDEDOWN :
					{
						if (paramy) channel.finevslup = paramy;

						channel.volume -= channel.finevslup;

						if (channel.volume < 0)
                        {
							channel.volume =0;
                        }

						channel.notectrl |= FMUSIC_VOLUME_PAN;
						break;
					}
#endif
#ifdef FMUSIC_XM_NOTEDELAY_ACTIVE
					case FMUSIC_XM_NOTEDELAY :
					{
						channel.volume = oldvolume;
						channel.freq   = oldfreq;
						channel.pan    = oldpan;
						channel.notectrl &= ~(FMUSIC_FREQ | FMUSIC_VOLUME_PAN | FMUSIC_TRIGGER);
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
				if (note.eparam < 0x20)
                {
					mod.speed = note.eparam;
                }
				else
                {
					FMUSIC_SetBPM(mod, note.eparam);
                }
				break;
			}
#endif
#ifdef FMUSIC_XM_SETGLOBALVOLUME_ACTIVE
			case FMUSIC_XM_SETGLOBALVOLUME :
			{
				mod.globalvolume = note.eparam;
				if (mod.globalvolume > 64)
                {
					mod.globalvolume=64;
                }
				channel.notectrl |= FMUSIC_VOLUME_PAN;
				break;
			}
#endif
#ifdef FMUSIC_XM_GLOBALVOLSLIDE_ACTIVE
			case FMUSIC_XM_GLOBALVOLSLIDE :
			{
				if (note.eparam)
                {
					mod.globalvsl = note.eparam;
                }
				break;
			}
#endif
#ifdef FMUSIC_XM_SETENVELOPEPOS_ACTIVE
			case FMUSIC_XM_SETENVELOPEPOS :
			{
                if (!(iptr->sample_header.volEnvFlags & FMUSIC_ENVELOPE_ON))
                {
					break;
                }

				int currpos = 0;

				// search and reinterpolate new envelope position
				while (note.eparam > iptr->sample_header.volEnvelope[(currpos+1)<<1] && currpos < iptr->sample_header.numVolPoints) currpos++;

				channel.envvolpos = currpos;

				// if it is at the last position, abort the envelope and continue last volume
				if (channel.envvolpos >= iptr->sample_header.numVolPoints - 1)
				{
					channel.envvol = iptr->sample_header.volEnvelope[((iptr->sample_header.numVolPoints -1)<<1)+1];
					channel.envvolstopped = true;
					break;
				}

				channel.envvolstopped = false;
				channel.envvoltick = note.eparam;

				int nextpos = channel.envvolpos + 1;

				int currtick = iptr->sample_header.volEnvelope[currpos << 1];				// get tick at this point
				int nexttick = iptr->sample_header.volEnvelope[nextpos << 1];				// get tick at next point

				int currvol = iptr->sample_header.volEnvelope[(currpos << 1) + 1] << 16;	// get VOL at this point << 16
				int nextvol = iptr->sample_header.volEnvelope[(nextpos << 1) + 1] << 16;	// get VOL at next point << 16

				// interpolate 2 points to find delta step
				int tickdiff = nexttick - currtick;
				if (tickdiff) channel.envvoldelta = (nextvol-currvol) / tickdiff;
				else		  channel.envvoldelta = 0;

				tickdiff = channel.envvoltick - currtick;

				channel.envvolfrac  = currvol + (channel.envvoldelta * tickdiff);
				channel.envvol = channel.envvolfrac >> 16;
				channel.envvolpos++;
				break;
			}
#endif
#ifdef FMUSIC_XM_PANSLIDE_ACTIVE
			case FMUSIC_XM_PANSLIDE :
			{
				if (note.eparam)
				{
					channel.panslide = note.eparam;
					channel.notectrl |= FMUSIC_VOLUME_PAN;
				}
				break;
			}
#endif
#ifdef FMUSIC_XM_MULTIRETRIG_ACTIVE
			case FMUSIC_XM_MULTIRETRIG:
			{
				if (note.eparam)
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
				if (note.eparam)
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
					channel.freq -= channel.xtraportaup;
				}
				else if (paramx == 2)
				{
					if (paramy)
                    {
						channel.xtraportadown = paramy;
                    }
					channel.freq += channel.xtraportadown;
				}
				break;
			}
#endif
		}
#endif
		//= INSTRUMENT VIBRATO ============================================================================
#ifdef FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE
		FMUSIC_XM_InstrumentVibrato(channel, iptr);	// this gets added to previous freqdeltas
#endif
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
void FMUSIC_UpdateXMEffects(FMUSIC_MODULE &mod)
{
    const auto& row = mod.pattern[mod.header.pattern_order[mod.order]].row(mod.row);

	for (int count = 0; count<mod.header.channels_count; count++)
	{
        FMUSIC_INSTRUMENT		*iptr;
		FSOUND_SAMPLE			*sptr;
		const FMUSIC_NOTE& note = row[count];
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
			if (iptr->sample_header.noteSmpNums[channel.note] >= 16)
            {
				sptr = &FMUSIC_DummySample;
            }
			else
            {
				sptr = &iptr->sample[iptr->sample_header.noteSmpNums[channel.note]];
            }

			if (!sptr)
            {
				sptr = &FMUSIC_DummySample;
		    }
		}

		unsigned char effect = note.effect;			// grab the effect number
		unsigned char paramx = note.eparam >> 4;		// grab the effect parameter x
		unsigned char paramy = note.eparam & 0xF;		// grab the effect parameter y

		channel.voldelta	= 0;				// this is for tremolo / tremor etc
		channel.freqdelta = 0;				// this is for vibrato / arpeggio etc
		channel.notectrl	= 0;

		//= PROCESS ENVELOPES ==========================================================================
#ifdef FMUSIC_XM_VOLUMEENVELOPE_ACTIVE
		if (iptr->sample_header.volEnvFlags & FMUSIC_ENVELOPE_ON && !channel.envvolstopped)
        {
			FMUSIC_XM_ProcessEnvelope(channel, &channel.envvolpos, &channel.envvoltick, iptr->sample_header.volEnvFlags, iptr->sample_header.numVolPoints, &iptr->sample_header.volEnvelope[0], iptr->sample_header.volLoopEnd, iptr->sample_header.volLoopStart, iptr->sample_header.volSustain, &channel.envvol, &channel.envvolfrac, &channel.envvolstopped, &channel.envvoldelta);
        }
#endif
#ifdef FMUSIC_XM_PANENVELOPE_ACTIVE
		if (iptr->sample_header.panEnvFlags & FMUSIC_ENVELOPE_ON && !channel.envpanstopped)
        {
			FMUSIC_XM_ProcessEnvelope(channel, &channel.envpanpos, &channel.envpantick, iptr->sample_header.panEnvFlags, iptr->sample_header.numPanPoints, &iptr->sample_header.panEnvelope[0], iptr->sample_header.panLoopEnd, iptr->sample_header.panLoopStart, iptr->sample_header.panSustain, &channel.envpan, &channel.envpanfrac, &channel.envpanstopped, &channel.envpandelta);
        }
#endif

		//= PROCESS VOLUME FADEOUT =====================================================================
		if (channel.keyoff)
		{
			channel.fadeoutvol -= iptr->sample_header.volFadeout;
			if (channel.fadeoutvol < 0)
            {
				channel.fadeoutvol = 0;
            }
			channel.notectrl |= FMUSIC_VOLUME_PAN;
		}

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
				if (channel.volume < 0)
                {
					channel.volume = 0;
                }
				channel.notectrl |= FMUSIC_VOLUME_PAN;
				break;
			}
			case 0x7 :
			{
				channel.volume += (note.volume & 0xF);
				if (channel.volume > 0x40)
                {
					channel.volume = 0x40;
                }
				channel.notectrl |= FMUSIC_VOLUME_PAN;
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
				channel.notectrl |= FMUSIC_VOLUME_PAN;
				break;
			}
			case 0xe :
			{
				channel.pan += (note.volume & 0xF);
				channel.notectrl |= FMUSIC_VOLUME_PAN;
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
				if (note.eparam > 0)
				{
					switch (mod.tick % 3)
					{
						case 1:
						    {
						        if (mod.header.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY)
						            channel.freqdelta = paramx << 6;
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
						        else
						            channel.freqdelta = FMUSIC_XM_GetAmigaPeriod(channel.realnote+paramx, sptr->header.finetune) -
                                                      FMUSIC_XM_GetAmigaPeriod(channel.realnote, sptr->header.finetune);
#endif
						        break;
						    }
						case 2:
						    {
						        if (mod.header.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY)
						            channel.freqdelta = paramy << 6;
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
						        else
						            channel.freqdelta = FMUSIC_XM_GetAmigaPeriod(channel.realnote+paramy, sptr->header.finetune) -
                                                      FMUSIC_XM_GetAmigaPeriod(channel.realnote, sptr->header.finetune);
#endif
						        break;
						    }
					}
					channel.notectrl |= FMUSIC_FREQ;
				}
				break;
			}
#endif
#ifdef FMUSIC_XM_PORTAUP_ACTIVE
			case FMUSIC_XM_PORTAUP :
			{
				channel.freqdelta = 0;

				channel.freq -= channel.portaup << 2; // subtract freq
				if (channel.freq < 56)
                {
					channel.freq=56;  // stop at B#8
                }
				channel.notectrl |= FMUSIC_FREQ;
				break;
			}
#endif
#ifdef FMUSIC_XM_PORTADOWN_ACTIVE
			case FMUSIC_XM_PORTADOWN :
			{
				channel.freqdelta = 0;

				channel.freq += channel.portadown << 2; // subtract freq
				channel.notectrl |= FMUSIC_FREQ;
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
					if (channel.volume > 64)
                    {
						channel.volume = 64;
				    }
				}
				else if (paramy)
				{
					channel.volume -= paramy;
					if (channel.volume < 0)
                    {
						channel.volume = 0;
				    }
				}

				channel.notectrl |= FMUSIC_VOLUME_PAN;
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
					if (channel.volume > 64)
                    {
						channel.volume = 64;
				}
				}
				else if (paramy)
				{
					channel.volume -= paramy;
					if (channel.volume < 0)
                    {
						channel.volume = 0;
				}
				}

				channel.notectrl |= FMUSIC_VOLUME_PAN;
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
					if (channel.volume > 64)
                    {
						channel.volume = 64;
				    }
				}
				else if (paramy)
				{
					channel.volume -= paramy;
					if (channel.volume < 0)
                    {
						channel.volume = 0;
				    }
				}

				channel.notectrl |= FMUSIC_VOLUME_PAN;
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
							channel.notectrl |= FMUSIC_VOLUME_PAN;
						}
						break;
					}
#endif
#ifdef FMUSIC_XM_RETRIG_ACTIVE
					case FMUSIC_XM_RETRIG :
					{
						if (!paramy)
                        {
							break; // divide by 0 bugfix
                        }
						if (!(mod.tick % paramy))
						{
							channel.notectrl |= FMUSIC_TRIGGER;
							channel.notectrl |= FMUSIC_VOLUME_PAN;
							channel.notectrl |= FMUSIC_FREQ;
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

							channel.freq = channel.period;
							channel.notectrl |= FMUSIC_FREQ;
#ifdef FMUSIC_XM_VOLUMEBYTE_ACTIVE
							if (note.volume)
                            {
								FMUSIC_XM_ProcessVolumeByte(channel, note.volume);
                            }
#endif
							channel.notectrl |= FMUSIC_TRIGGER;
						}
						else
						{
							channel.notectrl &= ~(FMUSIC_FREQ | FMUSIC_VOLUME_PAN | FMUSIC_TRIGGER);
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
						if (channel.volume > 64)
						{
						    channel.volume = 64;
						}
						if (channel.volume < 0)
						{
						    channel.volume = 0;
						}
						channel.notectrl |= FMUSIC_VOLUME_PAN;
					}
					channel.notectrl |= FMUSIC_TRIGGER;
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
					mod.globalvolume += paramx;
					if (mod.globalvolume > 64)
                    {
						mod.globalvolume = 64;
				    }
					channel.notectrl |= FMUSIC_VOLUME_PAN;
				}
				else if (paramy)
				{
					mod.globalvolume -= paramy;
					if (mod.globalvolume < 0)
                    {
						mod.globalvolume = 0;
				    }
					channel.notectrl |= FMUSIC_VOLUME_PAN;
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
					if (channel.pan > 255)
                    {
						channel.pan = 255;
				    }
				}
				else if (paramy)
				{
					channel.pan -= paramy;
					if (channel.pan < 0)
                    {
						channel.pan = 0;
				    }
				}

				channel.notectrl |= FMUSIC_VOLUME_PAN;
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

		//= INSTRUMENT VIBRATO ============================================================================
#ifdef FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE
		FMUSIC_XM_InstrumentVibrato(channel, iptr);	// this gets added to previous freqdeltas
#endif

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
void FMUSIC_UpdateXM(FMUSIC_MODULE &mod)
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
std::unique_ptr<FMUSIC_MODULE> FMUSIC_LoadXM(void* fp, SAMPLELOADCALLBACK samplecallback)
{
	auto mod = std::make_unique<FMUSIC_MODULE>();

    FSOUND_File_Seek(fp, 0, SEEK_SET);
	FSOUND_File_Read(&mod->header, sizeof(mod->header), fp);

	// seek to patterndata
    FSOUND_File_Seek(fp, 60L+mod->header.header_size, SEEK_SET);

	// unpack and read patterns
	for (int count=0; count < mod->header.patterns_count; count++)
	{
        unsigned short	patternsize, rows;
		unsigned int	patternHDRsize;
		unsigned char tempchar = 0;

		FSOUND_File_Read(&patternHDRsize, 4, fp);
		FSOUND_File_Read(&tempchar, 1, fp);
		FSOUND_File_Read(&rows, 2, fp);				// length of pattern

	    mod->pattern[count].set_rows(rows);

		FSOUND_File_Read(&patternsize, 2, fp);		// packed pattern size

		if (patternsize > 0)
		{
			for (int row = 0; row < rows; ++row)
			{
				auto &current_row = mod->pattern[count].row(row);

				for (int count2 = 0; count2 < mod->header.channels_count; count2++)
				{
                    unsigned char dat;

					FMUSIC_NOTE* nptr = &current_row[count2];

					FSOUND_File_Read(&dat, 1, fp);
					if (dat & 0x80)
					{
						if (dat & 1)  FSOUND_File_Read(&nptr->note, 1, fp);
						if (dat & 2)  FSOUND_File_Read(&nptr->number, 1, fp);
						if (dat & 4)  FSOUND_File_Read(&nptr->volume, 1, fp);
						if (dat & 8)  FSOUND_File_Read(&nptr->effect, 1, fp);
						if (dat & 16) FSOUND_File_Read(&nptr->eparam, 1, fp);
					}
					else
					{
						nptr->note = dat;

						FSOUND_File_Read(&nptr->number, 4, fp);

					}

					if (nptr->number > 0x80)
					{
						nptr->number = 0;
					}
				}
			}
		}
	}

	// load instrument information
	for (size_t count = 0; count < mod->header.instruments_count; ++count)
	{
        // point a pointer to that particular instrument
		FMUSIC_INSTRUMENT* iptr = &mod->instrument[count];

		int firstsampleoffset = FSOUND_File_Tell(fp);
		FSOUND_File_Read(&iptr->header, sizeof(iptr->header), fp);				// instrument size
		firstsampleoffset += iptr->header.instSize;

		assert(iptr->header.numSamples <= 16);

		if (iptr->header.numSamples > 0)
		{
			FSOUND_File_Read(&iptr->sample_header, sizeof(iptr->sample_header), fp);

			iptr->sample_header.volFadeout *= 2;		// i DONT KNOW why i needed this.. it just made it work

			if (iptr->sample_header.numVolPoints < 2)
            {
				iptr->sample_header.volEnvFlags = FMUSIC_ENVELOPE_OFF;
            }
			if (iptr->sample_header.numPanPoints < 2)
            {
				iptr->sample_header.panEnvFlags = FMUSIC_ENVELOPE_OFF;
            }

			// seek to first sample
			FSOUND_File_Seek(fp, firstsampleoffset, SEEK_SET);
			for (unsigned int count2 = 0; count2 < iptr->header.numSamples; count2++)
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

				if ((sptr->header.loop_mode == FSOUND_XM_LOOP_OFF) || (sptr->header.length == 0))
				{
					sptr->header.loop_start = 0;
					sptr->header.loop_length = sptr->header.length;
					sptr->header.loop_mode = FSOUND_XM_LOOP_OFF;
				}

			}

			// Load sample data
			for (unsigned int count2 = 0; count2 < iptr->header.numSamples; count2++)
			{
				FSOUND_SAMPLE	*sptr = &iptr->sample[count2];
				//unsigned int	samplelenbytes = sptr->header.length * sptr->bits / 8;

			    //= ALLOCATE MEMORY FOR THE SAMPLE BUFFER ==============================================

				if (sptr->header.length)
				{
					sptr->buff = new short[sptr->header.length + 8];

					if (samplecallback)
					{
						samplecallback(sptr->buff, sptr->header.length, count, count2);
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

							sptr->header.bits16 = 1;
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
					if (sptr->header.loop_mode == FSOUND_XM_LOOP_BIDI)
					{
						sptr->buff[sptr->header.loop_start+sptr->header.loop_length] = sptr->buff[sptr->header.loop_start+sptr->header.loop_length-1];// fix it
					}
					else if (sptr->header.loop_mode == FSOUND_XM_LOOP_NORMAL)
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
