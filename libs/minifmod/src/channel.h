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

#include <cstdint>

#include "envelope.h"
#include "instrument.h"
#include "lfo.h"
#include "portamento.h"
#include "sample.h"
#include "Sound.h"
#include "xmfile.h"

// Channel type - contains information on a mod channel
struct FMUSIC_CHANNEL
{
	unsigned char  	note;  				// last note set in channel

	bool			trigger;
	bool			stop;

	FSOUND_CHANNEL* cptr;				// pointer to FSOUND system mixing channel
	const Sample*	sptr;				// pointer to FSOUND system sample

	int				period;				// current mod frequency period for this channel
	int				volume;				// current mod volume for this channel
	int				pan;				// current mod pan for this channel
	int				voldelta;			// delta for volume commands.. tremolo/tremor etc
	int				period_delta;		// delta for frequency commands.. vibrato/arpeggio etc

	EnvelopeState   volume_envelope;
	EnvelopeState   pan_envelope;

	int				fadeoutvol;			// volume fade out
	int				ivibpos;   			// instrument vibrato position
	int				ivibsweeppos;		// instrument vibrato sweep position
	bool			keyoff;				// flag whether keyoff has been hit or not)

	unsigned char	inst;				// last instrument set in channel
	unsigned char  	realnote;  			// last realnote set in channel
	int				period_target;		// last period set in channel
	XMEffect		recenteffect;		// previous row's effect.. used to correct tremolo volume

	unsigned int	sampleoffset;		// sample offset for this channel in SAMPLES

	unsigned char	portadown;  		// last porta down value (XM)
	unsigned char	portaup;   			// last porta up value (XM)
	unsigned char	xtraportadown;		// last porta down value (XM)
	unsigned char	xtraportaup;  		// last porta up value (XM)
	int				volslide;   		// last volume slide value
	int				panslide;			// pan slide value
	unsigned char	retrigx;   			// last retrig volume slide used (XM + S3M)
	unsigned char	retrigy;   			// last retrig tick count used (XM + S3M)

	Portamento		portamento;			// note to porta to and speed

	LFO				vibrato;			// Vibrato LFO
	LFO				tremolo;			// Tremolo LFO

	unsigned char	tremorpos; 			// tremor position (XM + S3M)
	unsigned char 	tremoron;   		// remembered parameters for tremor (XM + S3M)
	unsigned char 	tremoroff;   		// remembered parameters for tremor (XM + S3M)
	uint8_t			patlooprow;
	uint8_t 		patloopno;  		// pattern loop variables for effect  E6x

	uint8_t			finevslup;			// parameter for fine volume slide up
	uint8_t			finevsldown;		// parameter for fine volume slide down
	uint8_t			fineportaup;		// parameter for fine porta slide up
	uint8_t			fineportadown;		// parameter for fine porta slide down
	int8_t			fine_tune;

	void processInstrument(const Instrument& instrument) noexcept;
	void reset(int volume, int pan) noexcept;
	void updateFlags(const Sample& sample, int globalvolume, bool linearfrequency) noexcept;
	void processVolumeByte(uint8_t volume_byte) noexcept;
	void tremor() noexcept;
};
