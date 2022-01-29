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

#ifndef SOUND_H_
#define SOUND_H_

#include <cstdint>

#ifdef WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#include "xmfile.h"

//= DEFINITIONS ===============================================================================

#define FSOUND_LATENCY	20

#define BLOCK_ON_SOFTWAREUPDATE() while(FSOUND_Software_UpdateMutex);

#define FSOUND_XM_LOOP_OFF 0
#define FSOUND_XM_LOOP_NORMAL	1		// For forward looping samples.
#define FSOUND_XM_LOOP_BIDI	2		// For bidirectional looping samples.  (no effect if in hardware).

// ==============================================================================================
// STRUCTURE DEFINITIONS
// ==============================================================================================

// Sample type - contains info on sample
struct FSOUND_SAMPLE final
{
	int16_t		 	*buff;			// pointer to sound data

	XMSampleHeader header;

	// music stuff
	unsigned char 	globalvol;    	// sample global volume (scalar)
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
struct FSOUND_CHANNEL
{
	int				index;			// position in channel pool.
	unsigned int 	sampleoffset; 	// sample offset (sample starts playing from here).

	FSOUND_SAMPLE	*sptr;			// currently playing sample

	// software mixer stuff
	float			leftvolume;     // mixing information. adjusted volume for left channel (panning involved)
	float			rightvolume;    // mixing information. adjusted volume for right channel (panning involved)
	bool			volume_changed; // mixing information. set to true every time volume is changed.
	float			mixpos;			// mixing information. floating point fractional position in sample. should this be a double?
	float			speed;			// mixing information. playback rate - floating point. should this be a double?
	unsigned int	speeddir;		// mixing information. playback direction - forwards or backwards

	// software mixer volume ramping stuff
	float			ramp_leftvolume;
	float			ramp_rightvolume;
	float			ramp_leftspeed;
	float			ramp_rightspeed;
	unsigned int	ramp_count;
};

//= FUNCTIONS =================================================================================

struct FSOUND_SoundBlock
{
#ifdef WIN32
	WAVEHDR		wavehdr;
#endif
	short		*data;
};

//= CONSTANT EXPRESSIONS ======================================================================
constexpr int			FSOUND_BufferSizeMs = 1000;


//= VARIABLE EXTERNS ==========================================================================
inline FSOUND_CHANNEL		FSOUND_Channel[64];			// channel pool
inline int					FSOUND_MixRate = 44100;		// mixing rate in hz.
#ifdef WIN32
inline HWAVEOUT				FSOUND_WaveOutHandle;
#endif
inline FSOUND_SoundBlock	FSOUND_MixBlock;

// mixing info
inline float*				FSOUND_MixBuffer;			// mix output buffer (stereo 32bit float)
inline int					FSOUND_BufferSize;			// size of 1 'latency' ms buffer in bytes
inline int					FSOUND_BlockSize;			// LATENCY ms worth of samples

// thread control variables
inline bool					FSOUND_Software_Exit			= false;		// mixing thread termination flag
inline bool					FSOUND_Software_UpdateMutex		= false;
inline bool					FSOUND_Software_ThreadFinished	= true;
#ifdef WIN32
inline HANDLE				FSOUND_Software_hThread			= NULL;
#endif
inline int					FSOUND_Software_FillBlock		= 0;
inline int					FSOUND_Software_RealBlock;

#ifdef WIN32
DWORD						FSOUND_Software_DoubleBufferThread(LPDWORD lpdwParam);
#endif
void						FSOUND_Software_Fill();

#endif
