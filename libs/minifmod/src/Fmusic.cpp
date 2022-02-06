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

#include <thread>

#include <minifmod/minifmod.h>
#include "music_formatxm.h"
#include "system_file.h"

FMUSIC_CHANNEL		FMUSIC_Channel[32];		// channel array for this song
FMUSIC_TIMMEINFO *	FMUSIC_TimeInfo;

static std::thread	Software_Thread;

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

void FMUSIC_SetBPM(FMUSIC_MODULE &module, int bpm) noexcept
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
    if (void* fp = FSOUND_File_Open(name))
    {
        // create a mod instance
        std::unique_ptr<FMUSIC_MODULE> mod = XMLoad(fp, sampleloadcallback);
        FSOUND_File_Close(fp);
        return mod.release();
    }
    return nullptr;
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
	if (mod)
    {
        FMUSIC_StopSong();
        delete mod;
        return true;
    }
    return false;
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

	mod->globalvolume = 64;
	mod->speed = (int)mod->header.default_tempo;
	mod->row = 0;
	mod->order = 0;
	mod->nextorder = -1;
	mod->nextrow = -1;
	mod->mixer_samplesleft = 0;
	mod->tick = 0;
	mod->patterndelay = 0;
	mod->time_ms = 0;

	FMUSIC_SetBPM(*mod, mod->header.default_bpm);

	memset(FMUSIC_Channel, 0, mod->header.channels_count * sizeof(FMUSIC_CHANNEL));
	//	memset(FSOUND_Channel, 0, 64 * sizeof(FSOUND_CHANNEL));

	for (uint16_t count = 0; count < mod->header.channels_count; count++)
	{
		FMUSIC_Channel[count].cptr = &FSOUND_Channel[count];
	}

    // ========================================================================================================
	// CREATE THREADS / TIMERS (last)
	// ========================================================================================================
	Software_Thread = std::thread(FSOUND_Software_DoubleBufferThread, mod);
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
	// wait until callback has settled down and exited
	if (Software_Thread.joinable())
	{
		// Kill the thread
		Software_Thread_Exit = true;
		Software_Thread.join();
	}
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
unsigned char FMUSIC_GetOrder() noexcept
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
unsigned char FMUSIC_GetRow() noexcept
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
unsigned int FMUSIC_GetTime() noexcept
{
	if (!FMUSIC_TimeInfo)
    {
		return 0;
    }

	return FMUSIC_TimeInfo[FSOUND_Software_RealBlock].ms;
}
