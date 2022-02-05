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

#include "Music.h"
#include "music_formatxm.h"

#ifdef WIN32
#define NOMINMAX
#include <Windows.h>
#endif

//= CONSTANT EXPRESSIONS ======================================================================
constexpr int	FSOUND_BufferSizeMs = 1000;
constexpr int	FSOUND_LATENCY = 20;

// mixing info
static float*				FSOUND_MixBuffer;			// mix output buffer (stereo 32bit float)
static int					FSOUND_BufferSize;			// size of 1 'latency' ms buffer in bytes
static int					FSOUND_BlockSize;			// LATENCY ms worth of samples
static int					FSOUND_Software_FillBlock = 0;

constexpr float VolumeFilterTimeConstant = 0.003f; // time constant (RC) of the volume IIR filter
static float volume_filter_k = 0.0f;


struct FSOUND_SoundBlock
{
#ifdef WIN32
	WAVEHDR		wavehdr;
#endif
	short* data;
};

static FSOUND_SoundBlock	FSOUND_MixBlock;

#ifdef WIN32
static HWAVEOUT			FSOUND_WaveOutHandle;
#endif

/*
[API]
[
	[DESCRIPTION]
	Size optimized version of the commented out clipper below

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
static inline void MixerClipCopy_Float32(int16_t* dest, const float* src, size_t len) noexcept
{
	assert(src);
	assert(dest);
	for (size_t count = 0; count < len << 1; count++)
	{
		*dest++ = (int16_t)std::clamp((int)*src++, (int)std::numeric_limits<int16_t>::min(), (int)std::numeric_limits<int16_t>::max());
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
static inline void Mixer_FPU_FilteredVolume(float* mixptr, int len) noexcept
{
	//==============================================================================================
	// LOOP THROUGH CHANNELS
	//==============================================================================================
	for (auto& channel : FSOUND_Channel)
	{
		int sample_index = 0;

		//==============================================================================================
		// LOOP THROUGH CHANNELS
		//==============================================================================================
		while (channel.sptr && len > sample_index)
		{

			float samples_to_mix;
			if (channel.speeddir == MixDir::Forwards)
			{
				samples_to_mix = channel.sptr->header.loop_start + channel.sptr->header.loop_length - channel.mixpos;
				if (samples_to_mix <= 0)
				{
					samples_to_mix = channel.sptr->header.length - channel.mixpos;
				}
			}
			else
			{
				samples_to_mix = channel.mixpos - channel.sptr->header.loop_start;
			}
			const int samples_to_mix_target = (int)ceil(samples_to_mix / channel.speed); // round up the division

			// =========================================================================================
			// the following code sets up a mix counter. it sees what will happen first, will the output buffer
			// end be reached first or will the end of the sample be reached first?
			// whatever is smallest will be the mixcount.
			int mix_count = std::min(len - sample_index, samples_to_mix_target);

			float speed = channel.speed;

			if (channel.speeddir != MixDir::Forwards)
			{
				speed = -speed;
			}

			//= SET UP VOLUME MULTIPLIERS ==================================================

			for (int i = 0; i < mix_count; ++i)
			{
				const uint32_t mixpos = (uint32_t)channel.mixpos;
				const float frac = channel.mixpos - mixpos;
				const float samp1 = channel.sptr->buff[mixpos];
				const float newsamp = (channel.sptr->buff[mixpos + 1] - samp1) * frac + samp1;
				mixptr[0 + (sample_index + i) * 2] += channel.filtered_leftvolume * newsamp;
				mixptr[1 + (sample_index + i) * 2] += channel.filtered_rightvolume * newsamp;
				channel.filtered_leftvolume += (channel.leftvolume - channel.filtered_leftvolume) * volume_filter_k;
				channel.filtered_rightvolume += (channel.rightvolume - channel.filtered_rightvolume) * volume_filter_k;
				channel.mixpos += speed;
			}

			sample_index += mix_count;

			//=============================================================================================
			// SWITCH ON LOOP MODE TYPE
			//=============================================================================================
			if (mix_count == samples_to_mix_target)
			{
				if (channel.sptr->header.loop_mode == XMLoopMode::Normal)
				{
					const uint32_t target = channel.sptr->header.loop_start + channel.sptr->header.loop_length;
					do
					{
						channel.mixpos -= channel.sptr->header.loop_length;
					} while (channel.mixpos >= target);
				}
				else if (channel.sptr->header.loop_mode == XMLoopMode::Bidi)
				{
					do {
						if (channel.speeddir != MixDir::Forwards)
						{
							//BidiBackwards
							channel.mixpos = 2 * channel.sptr->header.loop_start - channel.mixpos - 1;
							channel.speeddir = MixDir::Forwards;
							if (channel.mixpos < channel.sptr->header.loop_start + channel.sptr->header.loop_length)
							{
								break;
							}
						}
						//BidiForward
						channel.mixpos = 2 * (channel.sptr->header.loop_start + channel.sptr->header.loop_length) - channel.mixpos - 1;
						channel.speeddir = MixDir::Backwards;

					} while (channel.mixpos < channel.sptr->header.loop_start);
				}
				else
				{
					channel.mixpos = 0;
					channel.sptr = nullptr;
				}
			}
		}
	}
}

/*
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
*/
void FSOUND_Software_Fill(FMUSIC_MODULE &mod) noexcept
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

        // keep resetting the mix pointer to the beginning of this portion of the ring buffer
		float* MixPtr = mixbuffer;

		while (MixedSoFar < FSOUND_BlockSize)
		{
			if (!mod.mixer_samplesleft)
			{
				FMUSIC_UpdateXM(mod);	// update new mod tick
				mod.mixer_samplesleft = mod.mixer_samplespertick;
			}

            const int SamplesToMix = std::min(mod.mixer_samplesleft, FSOUND_BlockSize - MixedSoFar);

			Mixer_FPU_FilteredVolume(MixPtr, SamplesToMix);

			MixedSoFar	+= SamplesToMix;
			MixPtr		+= SamplesToMix*2;
			mod.mixer_samplesleft -= SamplesToMix;

		}

		mod.time_ms += MixedSoFar * 1000 / FSOUND_MixRate; // This is (and was before) approximated down by as much as 1ms per block

		FMUSIC_TimeInfo[FSOUND_Software_FillBlock].ms    = mod.time_ms;
		FMUSIC_TimeInfo[FSOUND_Software_FillBlock].row   = mod.row;
		FMUSIC_TimeInfo[FSOUND_Software_FillBlock].order = mod.order;
	}


	// ====================================================================================
	// CLIP AND COPY BLOCK TO OUTPUT BUFFER
	// ====================================================================================
    MixerClipCopy_Float32(FSOUND_MixBlock.data + (mixpos << 1), mixbuffer, FSOUND_BlockSize);

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
void FSOUND_Software_DoubleBufferThread(FMUSIC_MODULE *mod) noexcept
{
	FSOUND_Software_Exit = false;

// ========================================================================================================
// INITIALIZE SOFTWARE MIXER
// ========================================================================================================

	FSOUND_BlockSize = ((FSOUND_MixRate * FSOUND_LATENCY / 1000) + 3) & 0xFFFFFFFC;	// Number of *samples*
	FSOUND_BufferSize = FSOUND_BlockSize * (FSOUND_BufferSizeMs / FSOUND_LATENCY);	// make it perfectly divisible by granularity
	FSOUND_BufferSize <<= 1;	// double buffer

	volume_filter_k = 1.f / (1.f + FSOUND_MixRate * VolumeFilterTimeConstant);
	int totalblocks = FSOUND_BufferSize / FSOUND_BlockSize;

    FMUSIC_TimeInfo = new FMUSIC_TIMMEINFO[totalblocks]{};

	//=======================================================================================
	// ALLOC GLOBAL CHANNEL POOL
	//=======================================================================================
	memset(FSOUND_Channel, 0, sizeof(FSOUND_CHANNEL) * 64);

	// ========================================================================================================
	// SET UP CHANNELS
	// ========================================================================================================

	for (int count = 0; count < 64; count++)
	{
		FSOUND_Channel[count].index = count;
		FSOUND_Channel[count].speed = 1.0f;
	}

	// ========================================================================================================
	// PREPARE THE OUTPUT
	// ========================================================================================================
	{

#ifdef WIN32
		// ========================================================================================================
		// INITIALIZE WAVEOUT
		// ========================================================================================================
		WAVEFORMATEX	pcmwf;
		pcmwf.wFormatTag = WAVE_FORMAT_PCM;
		pcmwf.nChannels = 2;
		pcmwf.wBitsPerSample = 16;
		pcmwf.nBlockAlign = pcmwf.nChannels * pcmwf.wBitsPerSample / 8;
		pcmwf.nSamplesPerSec = FSOUND_MixRate;
		pcmwf.nAvgBytesPerSec = pcmwf.nSamplesPerSec * pcmwf.nBlockAlign;
		pcmwf.cbSize = 0;

		if (UINT hr = waveOutOpen(&FSOUND_WaveOutHandle, WAVE_MAPPER, &pcmwf, 0, 0, 0))
		{
			FSOUND_Software_Exit = true;
			return;
		}
#endif
	}

	{
		// CREATE AND START LOOPING WAVEOUT BLOCK

		const int length = FSOUND_BufferSize * 2; // stereo

		FSOUND_MixBlock.data = new short[length]; // TODO: Check Leak

#ifdef WIN32
		FSOUND_MixBlock.wavehdr.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
		FSOUND_MixBlock.wavehdr.lpData = (LPSTR)FSOUND_MixBlock.data;
		FSOUND_MixBlock.wavehdr.dwBufferLength = length * sizeof(short);
		FSOUND_MixBlock.wavehdr.dwBytesRecorded = 0;
		FSOUND_MixBlock.wavehdr.dwUser = 0;
		FSOUND_MixBlock.wavehdr.dwLoops = -1;
		waveOutPrepareHeader(FSOUND_WaveOutHandle, &FSOUND_MixBlock.wavehdr, sizeof(WAVEHDR));
#endif
	}

	// ========================================================================================================
	// ALLOCATE MIXBUFFER
	// ========================================================================================================

	FSOUND_MixBuffer = new float[FSOUND_BufferSize * 2];

	// ========================================================================================================
	// PREFILL THE MIXER BUFFER
	// ========================================================================================================

	do
	{
		FSOUND_Software_Fill(*mod);
	} while (FSOUND_Software_FillBlock);

	// ========================================================================================================
	// START THE OUTPUT
	// ========================================================================================================

#ifdef WIN32
	waveOutWrite(FSOUND_WaveOutHandle, &FSOUND_MixBlock.wavehdr, sizeof(WAVEHDR));
#endif

	while (!FSOUND_Software_Exit)
	{
        MMTIME	mmt;

		mmt.wType = TIME_BYTES;
		waveOutGetPosition(FSOUND_WaveOutHandle, &mmt, sizeof(MMTIME));

		const int cursorpos = (mmt.u.cb >> 2) % FSOUND_BufferSize;
		const int cursorblock = cursorpos / FSOUND_BlockSize;

		while (FSOUND_Software_FillBlock != cursorblock)
		{
			FSOUND_Software_Fill(*mod);

			++FSOUND_Software_RealBlock;
			if (FSOUND_Software_RealBlock >= totalblocks)
            {
				FSOUND_Software_RealBlock = 0;
            }
		}

		Sleep(5);
	}

	// remove the output mixbuffer
	if (FSOUND_MixBuffer)
	{
		delete[] FSOUND_MixBuffer;
		FSOUND_MixBuffer = nullptr;
	}

#ifdef WIN32
	if (FSOUND_MixBlock.wavehdr.lpData)
	{
		waveOutUnprepareHeader(FSOUND_WaveOutHandle, &FSOUND_MixBlock.wavehdr, sizeof(WAVEHDR));
		FSOUND_MixBlock.wavehdr.dwFlags &= ~WHDR_PREPARED;
		FSOUND_MixBlock.data = nullptr;
		FSOUND_MixBlock.wavehdr.lpData = nullptr;
	}
#endif
	delete[] FSOUND_MixBlock.data;

	// ========================================================================================================
	// SHUT DOWN OUTPUT DRIVER
	// ========================================================================================================
#ifdef WIN32
	waveOutReset(FSOUND_WaveOutHandle);

	waveOutClose(FSOUND_WaveOutHandle);
#endif
	if (FMUSIC_TimeInfo)
	{
		delete[] FMUSIC_TimeInfo;
		FMUSIC_TimeInfo = nullptr;
	}
	FSOUND_Software_FillBlock = 0;
	FSOUND_Software_RealBlock = 0;
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
