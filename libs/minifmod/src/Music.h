/******************************************************************************/
/* MUSIC.H                                                                    */
/* ----------------                                                           */
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/

#ifndef MUSIC_H_
#define MUSIC_H_

#include <algorithm>
#include <cassert>
#include <cmath>

#include <minifmod/minifmod.h>
#include "Sound.h"
#include "xmfile.h"

#define FMUSIC_KEYOFF						97

// pattern data type
class Pattern
{
	size_t size_;
	XMNote data_[256][32];
public:
	Pattern() noexcept : size_{ 64 }, data_{} {}

    [[nodiscard]] size_t size() const noexcept { return size_; }
	void resize(size_t size)
	{
		assert(size <= 256);
	    size_ = size;
	}

	[[nodiscard]] auto operator[](size_t row) const noexcept -> decltype(data_[row])
	{
		assert(row < size_);
		return data_[row];
	}

	[[nodiscard]] auto operator[](size_t row) noexcept -> decltype(data_[row])
	{
		assert(row < size_);
		return data_[row];
	}
};

struct FMUSIC_TIMMEINFO
{
	unsigned char order;
	unsigned char row;
	unsigned int  ms;
};

struct EnvelopePoint
{
	int position;
	float value;
	float delta;
};

struct EnvelopePoints
{
	uint8_t count;
	EnvelopePoint envelope[12];
};

// Multi sample extended instrument
struct Instrument final
{
	XMInstrumentHeader			header;
	XMInstrumentSampleHeader	sample_header;
	EnvelopePoints				volume_envelope;
	EnvelopePoints				pan_envelope;
	Sample						sample[16];		// 16 samples per instrument
};

enum class WaveControl : uint8_t
{
    Sine,
	SawTooth,
	Square,
	Random,
};

class LFO final
{
	WaveControl wave_control_;
	bool continue_;
	int position_;
	int speed_;
	int depth_;
public:

	void setup(int speed, int depth)
	{
		if (speed) {
			speed_ = speed;
		}
		if (depth)
		{
			depth_ = depth * 4;
		}
	}

	void setFlags(uint8_t flags)
	{
		wave_control_ = (WaveControl)(flags & 3);
		continue_ = (flags & 4) != 0;
	}

	void reset()
	{
		if (!continue_)
		{
			position_ = 0;
		}
	}

	void update()
	{
		position_ += speed_;
		while (position_ > 31)
		{
			position_ -= 64;
		}
	}

	[[nodiscard]] int operator () () const
	{
		switch (wave_control_)
		{
		case WaveControl::Sine:
			return -int(sinf((float)position_ * (2 * 3.141592f / 64.0f)) * depth_);
		case WaveControl::SawTooth:
			return -(position_ * 2 + 1) * depth_ / 63;
		default:
		//case WaveControl::Square:
		//case WaveControl::Random:
			return (position_ >= 0) ? -depth_ : depth_ ; // square
		}
	}
};

class Portamento
{
	int target_;
	int speed_;
public:
	void setup(int target, int speed = 0)
	{
		if (speed) speed_ = speed;
		target_ = target;
	}
	int operator ()(int period) noexcept
	{
		return std::clamp(target_, period - speed_, period + speed_);
	}

};

// Channel type - contains information on a mod channel
struct FMUSIC_CHANNEL
{
	unsigned char  	note;  				// last note set in channel

	bool			trigger;
	bool			stop;

	FSOUND_CHANNEL	*cptr;				// pointer to FSOUND system mixing channel
	const Sample	*sptr;				// pointer to FSOUND system sample

	int				period;				// current mod frequency period for this channel
	int				volume;				// current mod volume for this channel
	int				pan;				// current mod pan for this channel
	int				voldelta;			// delta for volume commands.. tremolo/tremor etc
	int				period_delta;		// delta for frequency commands.. vibrato/arpeggio etc

	int				envvolpos;			// envelope position - stopped if >= to the number of volume points
	float			envvolvalue;		// value for volume envelope

	int				envpanpos;			// envelope position - stopped if >= to the number of pan points
	float			envpanvalue;		// value for pan envelope

	int				fadeoutvol;			// volume fade out
	int				ivibpos;   			// instrument vibrato position
	int				ivibsweeppos;		// instrument vibrato sweep position
	bool			keyoff;				// flag whether keyoff has been hit or not)

	unsigned char	inst;				// last instrument set in channel
	unsigned char  	realnote;  			// last realnote set in channel
	int				period_target;		// last period set in channel
	uint8_t			recenteffect;		// previous row's effect.. used to correct tremolo volume

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
};

// Song type - contains info on song
struct FMUSIC_MODULE final
{
	XMHeader	header;
	Pattern		pattern[256];		// patterns array for this song
	Instrument	instrument[128];	// instrument array for this song (not used in MOD/S3M)
	int			mixer_samplesleft;
	int			mixer_samplespertick;

	int			globalvolume;		// global mod volume
	int			globalvsl;			// global mod volume
	int			tick;				// current mod tick
	int			speed;				// speed of song in ticks per row
	uint8_t		row;				// current row in pattern
	uint8_t		order;				// current song order position
	int			patterndelay;		// pattern delay counter
	uint8_t		nextrow;			// current row in pattern
	uint8_t		nextorder;			// current song order position
	int			time_ms;			// time passed in seconds since song started
};


//= VARIABLE EXTERNS ========================================================================
extern FMUSIC_CHANNEL			FMUSIC_Channel[];		// channel array for this song
extern FMUSIC_TIMMEINFO *		FMUSIC_TimeInfo;

//= FUNCTION DECLARATIONS ====================================================================

// private (internal functions)
void	FMUSIC_SetBPM(FMUSIC_MODULE &mod, int bpm) noexcept;

#endif
