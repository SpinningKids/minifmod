/******************************************************************************/
/* MINIFMOD.H                                                                 */
/* ----------------                                                           */
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company name.                       */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/

//==========================================================================================
// MINIFMOD Main header file. Copyright (c), Firelight Technologies, 2000-2004.
// Based on FMOD, copyright (c), Firelight Technologies, 2000-2004.
//==========================================================================================

#ifndef MINIFMOD_H_
#define MINIFMOD_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#define MINIFMOD_NOEXCEPT noexcept
#else
#define MINIFMOD_NOEXCEPT
#endif

//===============================================================================================
//= DEFINITIONS
//===============================================================================================

// fmod defined types
struct PlayerState;
struct Module;

//===============================================================================================
//= FUNCTION PROTOTYPES
//===============================================================================================

// ==================================
// Initialization / Global functions.
// ==================================
typedef void (*SAMPLELOADCALLBACK)(short *buff, size_t length_samples, int instno, int sampno);
typedef void (*FMUSIC_CALLBACK)(PlayerState *mod, unsigned char param);

// this must be called before FSOUND_Init!
void FSOUND_File_SetCallbacks(void*         (*OpenCallback)(const char *name),
                              void			(*CloseCallback)(void* handle),
                              int			(*ReadCallback)(void *buffer, int size, void* handle),
                              void			(*SeekCallback)(void* handle, int pos, int mode),
                              int			(*TellCallback)(void* handle)) noexcept;
bool FSOUND_Init(int mixrate, int vcmmode = 0) MINIFMOD_NOEXCEPT;
float FSOUND_TimeFromSamples() MINIFMOD_NOEXCEPT;

// =============================================================================================
// FMUSIC API
// =============================================================================================

// Song management / playback functions.
// =====================================

Module*         FMUSIC_LoadSong(const char *name, SAMPLELOADCALLBACK sampleloadcallback);
bool    		FMUSIC_FreeSong(Module* module);
PlayerState*    FMUSIC_PlaySong(Module* module);
Module*         FMUSIC_StopSong(PlayerState* player_state = nullptr);

// Runtime song information.
// =========================

unsigned char	FMUSIC_GetOrder() MINIFMOD_NOEXCEPT;
unsigned char	FMUSIC_GetRow() MINIFMOD_NOEXCEPT;
unsigned int	FMUSIC_GetTime() MINIFMOD_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif
