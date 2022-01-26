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

#ifndef _MUSIC_H
#define _MUSIC_H

#include <cassert>
#include <cstdint>

#include <minifmod/minifmod.h>
#include "Sound.h"

#define FMUSIC_MAXORDERS					256

#define FMUSIC_KEYCUT						254
#define FMUSIC_KEYOFF						97

#define FMUSIC_FREQ							1
#define FMUSIC_VOLUME_PAN					2
#define FMUSIC_TRIGGER						4
#define FMUSIC_STOP							8

#define FMUSIC_ENVELOPE_OFF					0
#define FMUSIC_ENVELOPE_ON					1
#define FMUSIC_ENVELOPE_SUSTAIN				2
#define FMUSIC_ENVELOPE_LOOP				4

// Single note type - contains info on 1 note in a pattern
struct FMUSIC_NOTE
{
	unsigned char	note;			// note to play at     (0-133) 132=none,133=keyoff
	unsigned char	number;			// sample being played (0-99)
	unsigned char	volume;			// volume column value (0-64)  255=no volume
	unsigned char	effect;			// effect number       (0-1Ah)
	unsigned char	eparam;			// effect parameter    (0-255)
};

// pattern data type
class FMUSIC_PATTERN
{
	int		rows_;
	FMUSIC_NOTE data_[256][32];
public:
	FMUSIC_PATTERN() : rows_{ 64 }, data_{} {}

    [[nodiscard]] int rows() const { return rows_; }
	void set_rows(int rows)
	{
		assert(rows <= 256);
	    rows_ = rows;
	}

	[[nodiscard]] auto row(int row) const -> decltype(data_[row])
	{
		assert(row < rows_);
		return data_[row];
	}

	[[nodiscard]] auto row(int row) -> decltype(data_[row])
	{
		assert(row < rows_);
		return data_[row];
	}
};

#pragma pack(push, 1)

struct FMUSIC_TIMMEINFO
{
	unsigned char order;
	unsigned char row;
	unsigned int  ms;
};

struct FMUSIC_XM_INSTSAMPHEADER {
	uint32_t	headerSize;             // sample header size
	uint8_t		noteSmpNums[96];        // sample numbers for notes
	uint16_t	volEnvelope[24];        // volume envelope points
	uint16_t	panEnvelope[24];        // panning envelope points
	uint8_t		numVolPoints;           // number of volume envelope points
	uint8_t		numPanPoints;           // number of panning env. points
	uint8_t		volSustain;             // volume sustain point
	uint8_t		volLoopStart;           // volume loop start point
	uint8_t		volLoopEnd;             // volume loop end point
	uint8_t		panSustain;             // panning sustain point
	uint8_t		panLoopStart;           // panning loop start point
	uint8_t		panLoopEnd;             // panning loop end point
	uint8_t		volEnvFlags;            // volume envelope flags
	uint8_t		panEnvFlags;            // panning envelope flags
	uint8_t		vibType;                // vibrato type
	uint8_t		vibSweep;               // vibrato sweep
	uint8_t		vibDepth;               // vibrato depth
	uint8_t		vibRate;                // vibrato rate
	uint16_t	volFadeout;             // volume fadeout
	uint16_t	reserved;
};

static_assert(sizeof(FMUSIC_XM_INSTSAMPHEADER) == 243 - 29);

struct FMUSIC_XM_INSTHEADER
{
	uint32_t	instSize;               // instrument size
	char		instName[22];           // instrument filename
	uint8_t		instType;               // instrument type (now 0)
	uint16_t	numSamples;             // number of samples in instrument
};

static_assert(sizeof(FMUSIC_XM_INSTHEADER) == 29);

#pragma pack(pop)

// Multi sample extended instrument
struct FMUSIC_INSTRUMENT final
{
	FMUSIC_XM_INSTHEADER		header;
	FMUSIC_XM_INSTSAMPHEADER	sample_header;
	FSOUND_SAMPLE				sample[16];		// 16 samples per instrument
};



// Channel type - contains information on a mod channel
struct FMUSIC_CHANNEL
{
	unsigned char  	note;  				// last note set in channel

	unsigned char	notectrl;			// flags for FSOUND

	FSOUND_CHANNEL	*cptr;				// pointer to FSOUND system mixing channel
	FSOUND_SAMPLE	*sptr;				// pointer to FSOUND system sample

	int				freq;				// current mod frequency period for this channel
	int				volume;				// current mod volume for this channel
	int				pan;				// current mod pan for this channel
	int				voldelta;			// delta for volume commands.. tremolo/tremor etc
	int				freqdelta;			// delta for frequency commands.. vibrato/arpeggio etc

	int				envvoltick;			// tick counter for envelope position
	int				envvolpos;			// envelope position
	int				envvolfrac;			// fractional interpolated envelope volume
	int				envvol;				// final interpolated envelope volume
	int				envvoldelta;		// delta step between points
	bool			envvolstopped;		// flag to say whether envelope has finished or not

	int				envpantick;			// tick counter for envelope position
	int				envpanpos;			// envelope position
	int				envpanfrac;			// fractional interpolated envelope pan
	int				envpan;				// final interpolated envelope pan
	int				envpandelta;		// delta step between points
	bool			envpanstopped;		// flag to say whether envelope has finished or not

	int				fadeoutvol;			// volume fade out
	int				ivibpos;   			// instrument vibrato position
	int				ivibsweeppos;		// instrument vibrato sweep position
	bool			keyoff;				// flag whether keyoff has been hit or not)

	unsigned char	inst;				// last instrument set in channel
	unsigned char  	realnote;  			// last realnote set in channel
	unsigned int	period;				// last period set in channel
	unsigned char	recenteffect;		// previous row's effect.. used to correct tremolo volume

	unsigned int	sampleoffset;		// sample offset for this channel in SAMPLES

	unsigned char	portadown;  		// last porta down value (XM)
	unsigned char	portaup;   			// last porta up value (XM)
	unsigned char	xtraportadown;		// last porta down value (XM)
	unsigned char	xtraportaup;  		// last porta up value (XM)
	unsigned char	volslide;   		// last volume slide value (XM + S3M)
	unsigned char	panslide;			// pan slide parameter (XM)
	unsigned char	retrigx;   			// last retrig volume slide used (XM + S3M)
	unsigned char	retrigy;   			// last retrig tick count used (XM + S3M)

	int				portatarget; 		// note to porta to
	unsigned char	portaspeed;			// porta speed

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

	unsigned char 	wavecontrol;		// waveform type for vibrato and tremolo (4bits each)

	unsigned char	finevslup;			// parameter for fine volume slide down
	unsigned char	fineportaup;		// parameter for fine porta slide up
	unsigned char	fineportadown;		// parameter for fine porta slide down
};

#pragma pack(push, 1)

struct FMUSIC_XM_HEADER
{
	char		header[17];
	char		module_name[20];
	char		id; // 0x01a
	char		tracker_name[20];
	uint16_t	tracker_version;
	uint32_t	header_size;
	uint16_t	song_length;
	uint16_t	restart_position;
	uint16_t	channels_count;
	uint16_t	patterns_count; // < 256
	uint16_t	instruments_count; // < 128
	uint16_t	flags; // 	0 - Linear frequency table / Amiga freq.table
	uint16_t	default_tempo;
	uint16_t	default_bpm;
	uint8_t		pattern_order[256];
};

static_assert(sizeof(FMUSIC_XM_HEADER) == 60 + 20 + 256); // xm header is 336 bytes long

#pragma pack(pop)

// Song type - contains info on song
struct FMUSIC_MODULE final
{
	FMUSIC_XM_HEADER	header;
	FMUSIC_PATTERN		pattern[256];	// patterns array for this song
	FMUSIC_INSTRUMENT	instrument[128];	// instrument array for this song (not used in MOD/S3M)
	int				mixer_samplesleft;
	int				mixer_samplespertick;

	int				globalvolume;		// global mod volume
	unsigned char	globalvsl;			// global mod volume
	int				tick;				// current mod tick
	int				speed;				// speed of song in ticks per row
	int				row;				// current row in pattern
	int				order;				// current song order position
	int				patterndelay;		// pattern delay counter
	int				nextrow;			// current row in pattern
	int				nextorder;			// current song order position
	int				time_ms;			// time passed in seconds since song started
};


//= VARIABLE EXTERNS ========================================================================
extern FMUSIC_MODULE *			FMUSIC_PlayingSong;
extern FSOUND_SAMPLE			FMUSIC_DummySample;
extern FSOUND_CHANNEL			FMUSIC_DummyChannel;
extern FMUSIC_INSTRUMENT		FMUSIC_DummyInstrument;
extern FMUSIC_CHANNEL			FMUSIC_Channel[];		// channel array for this song
extern FMUSIC_TIMMEINFO *		FMUSIC_TimeInfo;

//= FUNCTION DECLARATIONS ====================================================================

#define FMUSIC_PERIOD2HZ(_per) (14317056L / (_per))

// private (internal functions)
void	FMUSIC_SetBPM(FMUSIC_MODULE &mod, int bpm);

#endif
