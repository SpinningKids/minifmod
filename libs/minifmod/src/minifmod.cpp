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

#include <minixm/module.h>
#include <minixm/player_state.h>
#include <minixm/system_file.h>

namespace {
	unsigned int FSOUND_MixRate = 44100;
	PlayerState* FSOUND_last_player_state = nullptr;
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
	On success, a pointer to a PlayerState handle is returned.
	On failure, nullptr is returned.

	[SEE_ALSO]
	FMUSIC_FreeSong
]
*/
Module* FMUSIC_LoadSong(const char *name, SAMPLE_LOAD_CALLBACK sample_load_callback)
{
    if (void* fp = minifmod::file_access.open(name))
    {
        // create a mod instance
		auto mod = std::make_unique<Module>(minifmod::file_access, fp, sample_load_callback);
		minifmod::file_access.close(fp);
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
	FSOUND_last_player_state->start();
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
int FMUSIC_GetOrder() noexcept
{
	return FSOUND_last_player_state ? FSOUND_last_player_state->getTimeInfo().position.order : 0;
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
int FMUSIC_GetRow() noexcept
{
	return FSOUND_last_player_state ? FSOUND_last_player_state->getTimeInfo().position.row : 0;
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
int FMUSIC_GetTime() noexcept
{
	return static_cast<int>(FSOUND_last_player_state ? FSOUND_last_player_state->getTimeInfo().samples * 1000ull / FSOUND_MixRate : 0);
}

bool FSOUND_Init(int mixrate, int /* vcmmode */) noexcept
{
	FSOUND_MixRate = mixrate;
	return true;
}

void FSOUND_File_SetCallbacks(void* (*OpenCallback)(const char* name), void	(*CloseCallback)(void* handle), int (*ReadCallback)(void* buffer, int size, void* handle), void (*SeekCallback)(void*, int pos, int mode), int (*TellCallback)(void* handle)) noexcept
{
	minifmod::file_access.open = OpenCallback;
	minifmod::file_access.close = CloseCallback;
	minifmod::file_access.read = ReadCallback;
	minifmod::file_access.seek = SeekCallback;
	minifmod::file_access.tell = TellCallback;
}
