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
#include <memory>

#include "xmfile.h"

enum class MixDir
{
    Forwards,
	Backwards
};

// ==============================================================================================
// STRUCTURE DEFINITIONS
// ==============================================================================================

// Sample type - contains info on sample
struct Sample final
{
	XMSampleHeader header;

	std::unique_ptr<int16_t[]> buff;			// pointer to sound data
};

// Channel type - contains information on a mixing channel
struct FSOUND_CHANNEL
{
	int				index;			// position in channel pool.
	unsigned int 	sampleoffset; 	// sample offset (sample starts playing from here).

	const Sample	*sptr;			// currently playing sample

	// software mixer stuff
	float			leftvolume;     // mixing information. adjusted volume for left channel (panning involved)
	float			rightvolume;    // mixing information. adjusted volume for right channel (panning involved)
	float			mixpos;			// mixing information. floating point fractional position in sample.
	float			speed;			// mixing information. playback rate - floating point.
	MixDir			speeddir;		// mixing information. playback direction - forwards or backwards

	// software mixer volume ramping stuff
	float			filtered_leftvolume;
	float			filtered_rightvolume;
};

//= VARIABLE EXTERNS ==========================================================================
inline FSOUND_CHANNEL		FSOUND_Channel[64];			// channel pool
inline int					FSOUND_MixRate = 44100;		// mixing rate in hz.

// thread control variables
inline bool					Software_Thread_Exit			= false;		// mixing thread termination flag
inline int					FSOUND_Software_RealBlock;

struct FMUSIC_MODULE;
void						FSOUND_Software_DoubleBufferThread(FMUSIC_MODULE* mod) noexcept;
void						FSOUND_Software_Fill(FMUSIC_MODULE& mod) noexcept;

#endif
