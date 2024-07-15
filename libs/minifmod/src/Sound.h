/******************************************************************************/
/* SOUND.H                                                                    */
/* ----------------                                                           */
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/

#ifndef _SOUND_H_
#define _SOUND_H_

#include <cstdint>

#define NOMINMAX
#include <Windows.h>

//= DEFINITIONS ===============================================================================

#define FSOUND_LATENCY	20

#define BLOCK_ON_SOFTWAREUPDATE() while(FSOUND_Software_UpdateMutex);

/*
[DEFINE_START]
[
 	[NAME]
	FSOUND_MODES

	[DESCRIPTION]
	Sample description bitfields, OR them together for loading and describing samples.
]
*/
#define FSOUND_NORMAL		0x0			// Default sample type.  Loop off, 8bit mono, signed, not hardware accelerated.
#define FSOUND_LOOP_OFF		0x01		// For non looping samples.
#define FSOUND_LOOP_NORMAL	0x02		// For forward looping samples.
#define FSOUND_LOOP_BIDI	0x04		// For bidirectional looping samples.  (no effect if in hardware).
#define FSOUND_8BITS		0x08		// For 8 bit samples.
#define FSOUND_16BITS		0x10		// For 16 bit samples.
#define FSOUND_MONO			0x20		// For mono samples.
#define FSOUND_STEREO		0x40		// For stereo samples.
#define FSOUND_UNSIGNED		0x80		// For source data containing unsigned samples.
#define FSOUND_SIGNED		0x100		// For source data containing signed data.
#define FSOUND_DELTA		0x200		// For source data stored as delta values.
#define FSOUND_IT214		0x400		// For source data stored using IT214 compression.
#define FSOUND_IT215		0x800		// For source data stored using IT215 compression.
// [DEFINE_END]

// ==============================================================================================
// STRUCTURE DEFINITIONS
// ==============================================================================================

// Sample type - contains info on sample
struct FSOUND_SAMPLE final
{
	short		 	*buff;			// pointer to sound data

	// ====================================================================================
	// BUGFIX 1.5 (finetune was in wrong place)
	// DONT CHANGE THE ORDER OF THESE NEXT MEMBERS AS THEY ARE ALL LOADED IN A SINGLE READ
	unsigned int 	length;       	// sample length (samples)
	unsigned int 	loopstart;    	// sample loop start (samples)
	unsigned int 	looplen;      	// sample loop length (samples)
	unsigned char	defvol;    		// sample default volume
	signed char		finetune;		// finetune value from -128 to 127
	// ====================================================================================

	int 			deffreq;     	// sample default speed in hz
	int				defpan;       	// sample default pan

	unsigned char	bits;			// bits per sample
	unsigned char	loopmode;      	// loop type

	// music stuff
	unsigned char 	globalvol;    	// sample global volume (scalar)
	signed char		relative;	  	// relative note
	int				middlec;      	// finetuning adjustment to make for music samples.. relative to 8363hz
	unsigned int 	susloopbegin;  	// sample loop start
	unsigned int 	susloopend;    	// sample loop length
	unsigned char 	vibspeed;		// vibrato speed 0-64
	unsigned char	vibdepth;		// vibrato depth 0-64

	~FSOUND_SAMPLE()
    {
		delete[] buff;
	}
};


// Channel type - contains information on a mixing channel
typedef struct
{
	int				index;			// position in channel pool.
	int				volume;   		// current volume (00-FFh).
	int				pan;   			// panning value (00-FFh).
	int				actualvolume;   // driver level current volume.
	int				actualpan;   	// driver level panning value.
	unsigned int 	sampleoffset; 	// sample offset (sample starts playing from here).

	FSOUND_SAMPLE	*sptr;			// currently playing sample

	// software mixer stuff
	unsigned int	leftvolume;     // mixing information. adjusted volume for left channel (panning involved)
	unsigned int	rightvolume;    // mixing information. adjusted volume for right channel (panning involved)
	uint64_t		mixpos64;		// mixing information. 32:32 fractional position in sample
	uint64_t		speed64;		// mixing information. playback rate - 32:32
	unsigned int	speeddir;		// mixing information. playback direction - forwards or backwards

	// software mixer volume ramping stuff
	unsigned int	ramp_lefttarget;
	unsigned int	ramp_righttarget;
	unsigned int	ramp_leftvolume;
	unsigned int	ramp_rightvolume;
	float			ramp_leftspeed;
	float			ramp_rightspeed;
	unsigned int	ramp_count;
} FSOUND_CHANNEL;

//= FUNCTIONS =================================================================================

typedef struct
{
	WAVEHDR		wavehdr;
	short		*data;
} FSOUND_SoundBlock;

//= CONSTANT EXPRESSIONS ======================================================================
constexpr int			FSOUND_BufferSizeMs = 1000;


//= VARIABLE EXTERNS ==========================================================================
inline FSOUND_CHANNEL		FSOUND_Channel[64];			// channel pool
inline int					FSOUND_MixRate = 44100;		// mixing rate in hz.
inline HWAVEOUT				FSOUND_WaveOutHandle;
inline FSOUND_SoundBlock	FSOUND_MixBlock;

// mixing info
inline float*				FSOUND_MixBuffer;			// mix output buffer (stereo 32bit float)
inline int					FSOUND_BufferSize;			// size of 1 'latency' ms buffer in bytes
inline int					FSOUND_BlockSize;			// LATENCY ms worth of samples

// thread control variables
inline bool					FSOUND_Software_Exit			= false;		// mixing thread termination flag
inline bool					FSOUND_Software_UpdateMutex		= false;
inline bool					FSOUND_Software_ThreadFinished	= true;
inline HANDLE				FSOUND_Software_hThread			= NULL;
inline int					FSOUND_Software_FillBlock		= 0;
inline int					FSOUND_Software_RealBlock;

DWORD						FSOUND_Software_DoubleBufferThread(LPDWORD lpdwParam);
void						FSOUND_Software_Fill();

#endif
