/******************************************************************************/
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/
/* C++ conversion and (heavy) refactoring by Pan/SpinningKids, 2022           */
/******************************************************************************/

#pragma once

#include "envelope.h"
#include "instrument.h"
#include "lfo.h"
#include "mixer.h"
#include "portamento.h"
#include "xmeffects.h"

#include <xmformat/effect.h>

// Channel type - contains information on a mod channel
struct Channel
{
	int				index;

	XMNote			note;  				// last note set in channel

	bool			trigger;
	bool			stop;

	int				period;				// current mod frequency period for this channel
	int				period_delta;		// delta for frequency commands.. vibrato/arpeggio etc
	int				period_target;		// last period set in channel

    int				volume;				// current mod volume for this channel
	int				pan;				// current mod pan for this channel
	int				voldelta;			// delta for volume commands.. tremolo/tremor etc

	int				fadeoutvol;			// volume fade out
	bool			keyoff;				// flag whether keyoff has been hit or not)

	int				inst;				// last instrument set in channel
	XMNote		  	realnote;  			// last realnote set in channel
	XMEffect		recenteffect;		// previous row's effect.. used to correct tremolo volume

	unsigned int	sampleoffset;		// sample offset for this channel in SAMPLES
	int				fine_tune;

#ifdef FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE
	int				ivibpos;   			// instrument vibrato position
	int				ivibsweeppos;		// instrument vibrato sweep position
#endif

#ifdef FMUSIC_XM_VOLUMEENVELOPE_ACTIVE
	EnvelopeState   volume_envelope;
#endif

#ifdef FMUSIC_XM_PANENVELOPE_ACTIVE
	EnvelopeState   pan_envelope;
#endif

#ifdef FMUSIC_XM_TREMOLO_ACTIVE
	LFO				tremolo;			// Tremolo LFO
#endif

#ifdef FMUSIC_XM_TREMOR_ACTIVE
	int				tremorpos; 			// tremor position (XM + S3M)
	int			 	tremoron;   		// remembered parameters for tremor (XM + S3M)
	int			 	tremoroff;   		// remembered parameters for tremor (XM + S3M)
#endif

#if defined(FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_VOLUMESLIDE_ACTIVE)
	int				volslide;   		// last volume slide value
#endif

#ifdef FMUSIC_XM_PORTAUP_ACTIVE
	int				portaup;   			// last porta up value (XM)
#endif

#ifdef FMUSIC_XM_PORTADOWN_ACTIVE
	int				portadown;  		// last porta down value (XM)
#endif

#ifdef FMUSIC_XM_EXTRAFINEPORTA_ACTIVE
	int				xtraportadown;		// last porta down value (XM)
	int				xtraportaup;  		// last porta up value (XM)
#endif

#ifdef FMUSIC_XM_PANSLIDE_ACTIVE
	int				panslide;			// pan slide value
#endif

#ifdef FMUSIC_XM_MULTIRETRIG_ACTIVE
	int				retrigx;   			// last retrig volume slide used (XM + S3M)
	int				retrigy;   			// last retrig tick count used (XM + S3M)
#endif

#if defined(FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_PORTATO_ACTIVE) || defined(FMUSIC_XM_VOLUMEBYTE_ACTIVE)
	Portamento		portamento;			// note to porta to and speed
#endif

#if defined(FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_VIBRATO_ACTIVE) || defined(FMUSIC_XM_VOLUMEBYTE_ACTIVE)
	LFO				vibrato;			// Vibrato LFO
#endif

#ifdef FMUSIC_XM_FINEPORTAUP_ACTIVE
	int				fineportaup;		// parameter for fine porta slide up
#endif

#ifdef FMUSIC_XM_FINEPORTADOWN_ACTIVE
	int				fineportadown;		// parameter for fine porta slide down
#endif

#ifdef FMUSIC_XM_PATTERNLOOP_ACTIVE
	int				patlooprow;
	int		 		patloopno;  		// pattern loop variables for effect  E6x
#endif

#ifdef FMUSIC_XM_FINEVOLUMESLIDEUP_ACTIVE
	int				finevslup;			// parameter for fine volume slide up
#endif

#ifdef FMUSIC_XM_FINEVOLUMESLIDEDOWN_ACTIVE
    int				finevsldown;		// parameter for fine volume slide down
#endif

	void processInstrument(const Instrument& instrument) noexcept;
	void reset(int new_volume, int new_pan) noexcept;
	void processVolumeByteNote(int volume_byte) noexcept;
	void processVolumeByteTick(int volume_byte) noexcept;
	void tremor() noexcept;
	void updateVolume() noexcept;

	void setVolSlide(int value) noexcept { if (value) volslide = value; }
	void setPanSlide(int value) noexcept { if (value) panslide = value; }
	void setSampleOffset(int value) noexcept { if (value) sampleoffset = value; }
	void updatePeriodFromPortamento() noexcept
	{
#if defined(FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_PORTATO_ACTIVE) || defined(FMUSIC_XM_VOLUMEBYTE_ACTIVE)
		period = portamento(period);
#endif
	}

	void sendToMixer(Mixer& mixer, const Instrument& instrument, int global_volume, bool linear_frequency) const noexcept;
};
