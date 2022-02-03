/******************************************************************************/
/* FSOUND.C                                                                   */
/* ----------------                                                           */
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/

#include <algorithm>

#include "Sound.h"

#include <cstring>

#include "mixer_clipcopy.h"
#include "mixer_fpu_ramp.h"
#include "Music.h"
#include "music_formatxm.h"

/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
void FSOUND_Software_Fill() noexcept
{
    const int mixpos = FSOUND_Software_FillBlock * FSOUND_BlockSize;
	const int totalblocks = FSOUND_BufferSize / FSOUND_BlockSize;


	float * const mixbuffer = FSOUND_MixBuffer + (mixpos << 1);

	//==============================================================================
	// MIXBUFFER CLEAR
	//==============================================================================

	memset(mixbuffer, 0, FSOUND_BlockSize << 3);

	//==============================================================================
	// UPDATE MUSIC
	//==============================================================================

	{
		int MixedSoFar = 0;
		int MixedLeft = FMUSIC_PlayingSong->mixer_samplesleft;

        // keep resetting the mix pointer to the beginning of this portion of the ring buffer
		float* MixPtr = mixbuffer;

		while (MixedSoFar < FSOUND_BlockSize)
		{
			if (!MixedLeft)
			{
				FMUSIC_UpdateXM(*FMUSIC_PlayingSong);	// update new mod tick
				MixedLeft = FMUSIC_PlayingSong->mixer_samplespertick;
			}

            const int SamplesToMix = std::min(MixedLeft, FSOUND_BlockSize - MixedSoFar);

			FSOUND_Mixer_FPU_Ramp(MixPtr, SamplesToMix);

			MixedSoFar	+= SamplesToMix;
			MixPtr		+= SamplesToMix*2;
			MixedLeft	-= SamplesToMix;

		}

        FMUSIC_PlayingSong->time_ms += MixedSoFar * 1000 / FSOUND_MixRate; // This is (and was before) approximated down by as much as 1ms per block

		FMUSIC_TimeInfo[FSOUND_Software_FillBlock].ms    = FMUSIC_PlayingSong->time_ms;
		FMUSIC_TimeInfo[FSOUND_Software_FillBlock].row   = FMUSIC_PlayingSong->row;
		FMUSIC_TimeInfo[FSOUND_Software_FillBlock].order = FMUSIC_PlayingSong->order;

		FMUSIC_PlayingSong->mixer_samplesleft = MixedLeft;
	}


	// ====================================================================================
	// CLIP AND COPY BLOCK TO OUTPUT BUFFER
	// ====================================================================================
    FSOUND_MixerClipCopy_Float32(FSOUND_MixBlock.data + (mixpos << 1), mixbuffer, FSOUND_BlockSize);

	++FSOUND_Software_FillBlock;

	if (FSOUND_Software_FillBlock >= totalblocks)
    {
		FSOUND_Software_FillBlock = 0;
    }
}

/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
void FSOUND_Software_DoubleBufferThread() noexcept
{
    FSOUND_Software_ThreadFinished = false;

    const int totalblocks = FSOUND_BufferSize / FSOUND_BlockSize;

	while (!FSOUND_Software_Exit)
	{
        MMTIME	mmt;

		mmt.wType = TIME_BYTES;
		waveOutGetPosition(FSOUND_WaveOutHandle, &mmt, sizeof(MMTIME));

		const int cursorpos = (mmt.u.cb >> 2) % FSOUND_BufferSize;
		const int cursorblock = cursorpos / FSOUND_BlockSize;

		while (FSOUND_Software_FillBlock != cursorblock)
		{
			FSOUND_Software_UpdateMutex = true;

			FSOUND_Software_Fill();

			++FSOUND_Software_RealBlock;
			if (FSOUND_Software_RealBlock >= totalblocks)
            {
				FSOUND_Software_RealBlock = 0;
            }

			FSOUND_Software_UpdateMutex = false;
		}

		Sleep(5);
	}

	FSOUND_Software_ThreadFinished = true;
}

bool FSOUND_Init(int mixrate, int vcmmode) noexcept
{
	FSOUND_MixRate = mixrate;
	return true;
}

float FSOUND_TimeFromSamples() noexcept
{
#ifdef WIN32
	MMTIME mmtime;
	mmtime.wType = TIME_SAMPLES;
	waveOutGetPosition(FSOUND_WaveOutHandle, &mmtime, sizeof(mmtime));
	return mmtime.u.sample / (float)FSOUND_MixRate;
#else
	return 0;
#endif
}
