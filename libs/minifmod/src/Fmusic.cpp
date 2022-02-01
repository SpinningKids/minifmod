/******************************************************************************/
/* FMUSIC.C                                                                   */
/* ----------------                                                           */
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/

#include "Music.h"

#include <cstring>

#include <minifmod/minifmod.h>
#include "mixer_fpu_ramp.h"
#include "music_formatxm.h"
#include "system_file.h"

FMUSIC_MODULE *		FMUSIC_PlayingSong = nullptr;
FMUSIC_CHANNEL		FMUSIC_Channel[32];		// channel array for this song
FMUSIC_TIMMEINFO *	FMUSIC_TimeInfo;
FSOUND_SAMPLE		FMUSIC_DummySample;
// initialization taken out due to size.. should be ok.
/* =
{
//	0,								// index
	nullptr,						// buff

	0,								// length
	0,								// loopstart
	0,								// looplen
	0,								// default volume
	0,								// finetune value from -128 to 127

	44100,							// default frequency
	128,							// default pan

	8,								// bits
	FSOUND_LOOP_OFF,				// loop mode


	TRUE,							// music owned
	0,								// sample global volume (scalar)
	0,								// relative note
	8363,							// finetuning adjustment to make for music samples.. relative to 8363hz
	0,								// sample loop start
	0,								// sample loop length
	0,								// vibrato speed 0-64
	0,								// vibrato depth 0-64
	0,								// vibrato type 0=sine, 1=rampdown, 2=square, 3=random
	0,								// vibrato rate 0-64 (like sweep?)
};
*/

FSOUND_CHANNEL			FMUSIC_DummyChannel;
FMUSIC_INSTRUMENT		FMUSIC_DummyInstrument;

//= PRIVATE FUNCTIONS ==============================================================================


/*
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/

void FMUSIC_SetBPM(FMUSIC_MODULE &module, int bpm)
{
	// number of samples
	module.mixer_samplespertick = FSOUND_MixRate * 5 / (bpm * 2);
}


//= API FUNCTIONS ==============================================================================

/*
[API]
[
	[DESCRIPTION]
	To load a module with a given filename.  Supports loading of
	.XM  (fasttracker 2 modules)

	[PARAMETERS]
	'name'		Filename of module to load.

	[RETURN_VALUE]
	On success, a pointer to a FMUSIC_MODULE handle is returned.
	On failure, nullptr is returned.

	[SEE_ALSO]
	FMUSIC_FreeSong
]
*/
FMUSIC_MODULE * FMUSIC_LoadSong(const char *name, SAMPLELOADCALLBACK sampleloadcallback)
{
    void* fp = FSOUND_File_Open(name);
    if (!fp)
    {
        return nullptr;
    }

	// create a mod instance
	std::unique_ptr<FMUSIC_MODULE> mod = FMUSIC_LoadXM(fp, sampleloadcallback);

    FSOUND_File_Close(fp);

	return mod.release();
}


/*
[API]
[
	[DESCRIPTION]
	Frees memory allocated for a song and removes it from the FMUSIC system.

	[PARAMETERS]
	'mod'		Pointer to the song to be freed.

	[RETURN_VALUE]
	void

	[REMARKS]

	[SEE_ALSO]
	FMUSIC_LoadSong
]
*/
bool FMUSIC_FreeSong(FMUSIC_MODULE *mod)
{

	if (!mod)
		return false;

	BLOCK_ON_SOFTWAREUPDATE();

	FMUSIC_StopSong();

	delete mod;

	return true;
}

/*
[API]
[
	[DESCRIPTION]
	Starts a song playing.

	[PARAMETERS]
	'mod'		Pointer to the song to be played.

	[RETURN_VALUE]
	true		song succeeded playing
	false		song failed playing

	[REMARKS]

	[SEE_ALSO]
	FMUSIC_StopSong
]
*/
bool FMUSIC_PlaySong(FMUSIC_MODULE *mod)
{
    if (!mod)
    {
		return false;
    }

	FMUSIC_StopSong();

	if (!FSOUND_File_Open || !FSOUND_File_Close || !FSOUND_File_Read || !FSOUND_File_Seek || !FSOUND_File_Tell)
    {
		return false;
    }

	// ========================================================================================================
	// INITIALIZE SOFTWARE MIXER
	// ========================================================================================================

	FSOUND_BlockSize    = ((FSOUND_MixRate * FSOUND_LATENCY / 1000) + 3) & 0xFFFFFFFC;	// Number of *samples*
	FSOUND_BufferSize   = FSOUND_BlockSize * (FSOUND_BufferSizeMs / FSOUND_LATENCY);	// make it perfectly divisible by granularity
	FSOUND_BufferSize <<= 1;	// double buffer

	mix_filter_k    = 1.f/(1.f + FSOUND_MixRate * VolumeFilterTimeConstant);
	int totalblocks = FSOUND_BufferSize / FSOUND_BlockSize;

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

	mod->globalvolume       = 64;
 	mod->speed              = (int)mod->header.default_tempo;
	mod->row                = 0;
	mod->order              = 0;
	mod->nextorder          = -1;
	mod->nextrow            = -1;
	mod->mixer_samplesleft  = 0;
	mod->tick               = 0;
	mod->patterndelay       = 0;
	mod->time_ms            = 0;

	FMUSIC_SetBPM(*mod, mod->header.default_bpm);

	memset(FMUSIC_Channel, 0, mod->header.channels_count * sizeof(FMUSIC_CHANNEL));
//	memset(FSOUND_Channel, 0, 64 * sizeof(FSOUND_CHANNEL));

	for (uint16_t count=0; count < mod->header.channels_count; count++)
	{
		FMUSIC_Channel[count].cptr = &FSOUND_Channel[count];
	}

	FMUSIC_PlayingSong = mod;

	FMUSIC_TimeInfo = new FMUSIC_TIMMEINFO[totalblocks]{};

	// ========================================================================================================
	// PREPARE THE OUTPUT
	// ========================================================================================================
	{

#ifdef WIN32
		// ========================================================================================================
		// INITIALIZE WAVEOUT
		// ========================================================================================================
		WAVEFORMATEX	pcmwf;
		pcmwf.wFormatTag		= WAVE_FORMAT_PCM;
		pcmwf.nChannels			= 2;
		pcmwf.wBitsPerSample	= 16;
		pcmwf.nBlockAlign		= pcmwf.nChannels * pcmwf.wBitsPerSample / 8;
		pcmwf.nSamplesPerSec	= FSOUND_MixRate;
		pcmwf.nAvgBytesPerSec	= pcmwf.nSamplesPerSec * pcmwf.nBlockAlign;
		pcmwf.cbSize			= 0;

        if (UINT hr = waveOutOpen(&FSOUND_WaveOutHandle, WAVE_MAPPER, &pcmwf, 0, 0, 0))
		{
			return false;
	    }
#endif
	}

	{
        // CREATE AND START LOOPING WAVEOUT BLOCK

        const int length = FSOUND_BufferSize * 2; // stereo

		FSOUND_MixBlock.data = new short[length]; // TODO: Check Leak

#ifdef WIN32
		FSOUND_MixBlock.wavehdr.dwFlags			= WHDR_BEGINLOOP | WHDR_ENDLOOP;
		FSOUND_MixBlock.wavehdr.lpData				= (LPSTR)FSOUND_MixBlock.data;
		FSOUND_MixBlock.wavehdr.dwBufferLength		= length*sizeof(short);
		FSOUND_MixBlock.wavehdr.dwBytesRecorded	= 0;
		FSOUND_MixBlock.wavehdr.dwUser				= 0;
		FSOUND_MixBlock.wavehdr.dwLoops			= -1;
		waveOutPrepareHeader(FSOUND_WaveOutHandle, &FSOUND_MixBlock.wavehdr, sizeof(WAVEHDR));
#endif
	}

	// ========================================================================================================
	// ALLOCATE MIXBUFFER
	// ========================================================================================================

	FSOUND_MixBuffer = new float[FSOUND_BufferSize*2];


	// ========================================================================================================
	// PREFILL THE MIXER BUFFER
	// ========================================================================================================

	do
	{
		FSOUND_Software_Fill();
	} while (FSOUND_Software_FillBlock);

	// ========================================================================================================
	// START THE OUTPUT
	// ========================================================================================================

#ifdef WIN32
	waveOutWrite(FSOUND_WaveOutHandle, &FSOUND_MixBlock.wavehdr, sizeof(WAVEHDR));

	{
		DWORD	FSOUND_dwThreadId;

		// ========================================================================================================
		// CREATE THREADS / TIMERS (last)
		// ========================================================================================================
		FSOUND_Software_Exit = false;

		FSOUND_Software_hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)FSOUND_Software_DoubleBufferThread, nullptr,0, &FSOUND_dwThreadId);

		SetThreadPriority(FSOUND_Software_hThread, THREAD_PRIORITY_TIME_CRITICAL);	// THREAD_PRIORITY_HIGHEST);
	}
#endif
	return true;
}


/*
[API]
[
	[DESCRIPTION]
	Stops a song from playing.

	[PARAMETERS]
	'mod'		Pointer to the song to be stopped.

	[RETURN_VALUE]
	void

	[REMARKS]

	[SEE_ALSO]
	FMUSIC_PlaySong
]
*/
void FMUSIC_StopSong()
{
	// Kill the thread
	FSOUND_Software_Exit = true;

	// wait until callback has settled down and exited
	BLOCK_ON_SOFTWAREUPDATE();

#ifdef WIN32
	if (FSOUND_Software_hThread)
	{
		while (!FSOUND_Software_ThreadFinished)
        {
			Sleep(10);
        }
		FSOUND_Software_hThread = nullptr;
	}
#endif

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
		delete[] FSOUND_MixBlock.data;
		FSOUND_MixBlock.data = nullptr;
        FSOUND_MixBlock.wavehdr.lpData = nullptr;
    }
#endif

	FMUSIC_PlayingSong = nullptr;

	if (FMUSIC_TimeInfo)
    {
		delete[] FMUSIC_TimeInfo;
        FMUSIC_TimeInfo = nullptr;
    }

	// ========================================================================================================
	// SHUT DOWN OUTPUT DRIVER
	// ========================================================================================================
#ifdef WIN32
	waveOutReset(FSOUND_WaveOutHandle);

	waveOutClose(FSOUND_WaveOutHandle);
#endif

	FSOUND_Software_FillBlock = 0;
    FSOUND_Software_RealBlock = 0;
}

//= INFORMATION FUNCTIONS ======================================================================

/*
[API]
[
	[DESCRIPTION]
	Returns the song's current order number

	[PARAMETERS]
	'mod'		Pointer to the song to retrieve current order number from.

	[RETURN_VALUE]
	The song's current order number.
	On failure, 0 is returned.

	[REMARKS]

	[SEE_ALSO]
	FMUSIC_GetPattern
]
*/
unsigned char FMUSIC_GetOrder()
{
	if (!FMUSIC_TimeInfo)
	{
		return 0;
	}

	return FMUSIC_TimeInfo[FSOUND_Software_RealBlock].order;
}


/*
[API]
[
	[DESCRIPTION]
	Returns the song's current row number.

	[PARAMETERS]
 	'mod'		Pointer to the song to retrieve current row from.

	[RETURN_VALUE]
	On success, the song's current row number is returned.
	On failure, 0 is returned.

	[REMARKS]

	[SEE_ALSO]
]
*/
unsigned char FMUSIC_GetRow()
{
	if (!FMUSIC_TimeInfo)
    {
		return 0;
    }

	return FMUSIC_TimeInfo[FSOUND_Software_RealBlock].row;
}


/*
[API]
[
	[DESCRIPTION]
	Returns the time in milliseconds since the song was started.  This is useful for
	synchronizing purposes because it will be exactly the same every time, and it is
	reliably retriggered upon starting the song.  Trying to synchronize using other
	windows timers can lead to varying results, and inexact performance.  This fixes that
	problem by actually using the number of samples sent to the soundcard as a reference.

	[PARAMETERS]
	'mod'		Pointer to the song to get time from.

	[RETURN_VALUE]
	On success, the time played in milliseconds is returned.
	On failure, 0 is returned.

	[REMARKS]

	[SEE_ALSO]
]
*/
unsigned int FMUSIC_GetTime()
{
	if (!FMUSIC_TimeInfo)
    {
		return 0;
    }

	return FMUSIC_TimeInfo[FSOUND_Software_RealBlock].ms;
}
