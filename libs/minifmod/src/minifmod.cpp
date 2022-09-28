/******************************************************************************/
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/
/* C++ conversion and (heavy) refactoring by Pan/SpinningKids, 2022           */
/******************************************************************************/

#include <minifmod/minifmod.h>

#include <thread>

#include "module.h"
#include "player_state.h"
#include "system_file.h"

static int FSOUND_MixRate = 44100;
static PlayerState* FSOUND_last_player_state = nullptr;
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
	On success, a pointer to a PlayerState handle is returned.
	On failure, nullptr is returned.

	[SEE_ALSO]
	FMUSIC_FreeSong
]
*/
Module* FMUSIC_LoadSong(const char *name, SAMPLELOADCALLBACK sampleloadcallback)
{
    if (void* fp = FSOUND_File.open(name))
    {
        // create a mod instance
		std::unique_ptr<Module> mod{ new Module(FSOUND_File, fp, sampleloadcallback) };
        FSOUND_File.close(fp);
        return mod.release();
    }
    return {};
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
bool FMUSIC_FreeSong(Module* module)
{
	if (module)
	{
		delete module;
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
PlayerState* FMUSIC_PlaySong(Module* module)
{
	if (!module)
	{
		return nullptr;
	}

	FSOUND_last_player_state = new PlayerState(std::unique_ptr<Module>{ module }, FSOUND_MixRate);
	return FSOUND_last_player_state;
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
Module* FMUSIC_StopSong(PlayerState* player_state)
{
	std::unique_ptr<Module> module;
	if (!player_state) player_state = FSOUND_last_player_state;
	if (player_state)
	{
		module = player_state->stop();
		if (FSOUND_last_player_state == player_state) FSOUND_last_player_state = nullptr;
		delete player_state;
	}
	return module.release();
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
	return FSOUND_last_player_state ? FSOUND_last_player_state->getMixer().getTimeInfo().position.order : 0;
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
	return FSOUND_last_player_state ? FSOUND_last_player_state->getMixer().getTimeInfo().position.row : 0;
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
	return FSOUND_last_player_state ? FSOUND_last_player_state->getMixer().getTimeInfo().samples * 1000ull / FSOUND_MixRate : 0;
}

bool FSOUND_Init(int mixrate, int vcmmode) noexcept
{
	FSOUND_MixRate = mixrate;
	return true;
}

float FSOUND_TimeFromSamples() noexcept
{
	return FSOUND_last_player_state ? FSOUND_last_player_state->getMixer().timeFromSamples() : 0;
}
