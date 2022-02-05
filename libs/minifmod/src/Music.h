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

#include <cassert>

#include <minifmod/minifmod.h>
#include "Sound.h"
#include "xmfile.h"

#define FMUSIC_KEYOFF						97

// pattern data type
class FMUSIC_PATTERN
{
	int		rows_;
	XMNote data_[256][32];
public:
	FMUSIC_PATTERN() noexcept : rows_{ 64 }, data_{} {}

    [[nodiscard]] int rows() const noexcept { return rows_; }
	void set_rows(int rows)
	{
		assert(rows <= 256);
	    rows_ = rows;
	}

	[[nodiscard]] auto row(int row) const noexcept -> decltype(data_[row])
	{
		assert(row < rows_);
		return data_[row];
	}

	[[nodiscard]] auto row(int row) noexcept -> decltype(data_[row])
	{
		assert(row < rows_);
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
struct FMUSIC_INSTRUMENT final
{
	XMInstrumentHeader			header;
	XMInstrumentSampleHeader	sample_header;
	EnvelopePoints				volume_envelope;
	EnvelopePoints				pan_envelope;
	FSOUND_SAMPLE				sample[16];		// 16 samples per instrument
};

enum class WaveControl : uint8_t
{
    Sine,
	SawTooth,
	Square,
	Random,
};

// Channel type - contains information on a mod channel
struct FMUSIC_CHANNEL
{
	unsigned char  	note;  				// last note set in channel

	bool			trigger;
	bool			stop;

	FSOUND_CHANNEL	*cptr;				// pointer to FSOUND system mixing channel
	const FSOUND_SAMPLE	*sptr;				// pointer to FSOUND system sample

	int				period;				// current mod frequency period for this channel
	int				volume;				// current mod volume for this channel
	int				pan;				// current mod pan for this channel
	int				voldelta;			// delta for volume commands.. tremolo/tremor etc
	int				period_delta;		// delta for frequency commands.. vibrato/arpeggio etc

	int				envvolpos;			// envelope position - stopped if >= to the number of volume points
	EnvelopePoint   envvol;				// tick, fracvalue and fracdelta for volume envelope

	int				envpanpos;			// envelope position - stopped if >= to the number of pan points
	EnvelopePoint   envpan;				// tick, fracvalue and fracdelta for pan envelope

	int				fadeoutvol;			// volume fade out
	int				ivibpos;   			// instrument vibrato position
	int				ivibsweeppos;		// instrument vibrato sweep position
	bool			keyoff;				// flag whether keyoff has been hit or not)

	unsigned char	inst;				// last instrument set in channel
	unsigned char  	realnote;  			// last realnote set in channel
	int				period_target;		// last period set in channel
	unsigned char	recenteffect;		// previous row's effect.. used to correct tremolo volume

	unsigned int	sampleoffset;		// sample offset for this channel in SAMPLES

	unsigned char	portadown;  		// last porta down value (XM)
	unsigned char	portaup;   			// last porta up value (XM)
	unsigned char	xtraportadown;		// last porta down value (XM)
	unsigned char	xtraportaup;  		// last porta up value (XM)
	int				volslide;   		// last volume slide value
	int				panslide;			// pan slide value
	unsigned char	retrigx;   			// last retrig volume slide used (XM + S3M)
	unsigned char	retrigy;   			// last retrig tick count used (XM + S3M)

	int				portatarget; 		// note to porta to
	int				portaspeed;			// porta speed

	signed char		vibpos;   			// vibrato position
	unsigned char	vibspeed;  			// vibrato speed
	unsigned char	vibdepth;  			// vibrato depth

	signed char		tremolopos;   		// tremolo position
	unsigned char  	tremolospeed; 		// tremolo speed
	unsigned char  	tremolodepth; 		// tremolo depth

	unsigned char	tremorpos; 			// tremor position (XM + S3M)
	unsigned char 	tremoron;   		// remembered parameters for tremor (XM + S3M)
	unsigned char 	tremoroff;   		// remembered parameters for tremor (XM + S3M)
 	int				patlooprow;
 	int 			patloopno;  		// pattern loop variables for effect  E6x

	WaveControl 	wavecontrol_vibrato;// waveform type for vibrato (2bits)
	bool			continue_vibrato;
	WaveControl		wavecontrol_tremolo;// waveform type for tremolo (2bits)
	bool			continue_tremolo;

	unsigned char	finevslup;			// parameter for fine volume slide up
	unsigned char	finevsldown;		// parameter for fine volume slide down
	unsigned char	fineportaup;		// parameter for fine porta slide up
	unsigned char	fineportadown;		// parameter for fine porta slide down
    int				finetune;
};

// Song type - contains info on song
struct FMUSIC_MODULE final
{
	XMHeader	header;
	FMUSIC_PATTERN		pattern[256];	// patterns array for this song
	FMUSIC_INSTRUMENT	instrument[128];	// instrument array for this song (not used in MOD/S3M)
	int				mixer_samplesleft;
	int				mixer_samplespertick;

	int				globalvolume;		// global mod volume
	int				globalvsl;			// global mod volume
	int				tick;				// current mod tick
	int				speed;				// speed of song in ticks per row
	uint8_t			row;				// current row in pattern
	uint8_t			order;				// current song order position
	int				patterndelay;		// pattern delay counter
	int				nextrow;			// current row in pattern
	int				nextorder;			// current song order position
	int				time_ms;			// time passed in seconds since song started
};


//= VARIABLE EXTERNS ========================================================================
extern FSOUND_SAMPLE			FMUSIC_DummySample;
extern FSOUND_CHANNEL			FMUSIC_DummyChannel;
extern FMUSIC_INSTRUMENT		FMUSIC_DummyInstrument;
extern FMUSIC_CHANNEL			FMUSIC_Channel[];		// channel array for this song
extern FMUSIC_TIMMEINFO *		FMUSIC_TimeInfo;

//= FUNCTION DECLARATIONS ====================================================================

// private (internal functions)
void	FMUSIC_SetBPM(FMUSIC_MODULE &mod, int bpm) noexcept;

#endif
