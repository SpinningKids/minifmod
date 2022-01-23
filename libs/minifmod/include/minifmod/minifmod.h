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

#ifndef _MINIFMOD_H_
#define _MINIFMOD_H_

//===============================================================================================
//= DEFINITIONS
//===============================================================================================

// fmod defined types
struct FMUSIC_MODULE;

//===============================================================================================
//= FUNCTION PROTOTYPES
//===============================================================================================

// ==================================
// Initialization / Global functions.
// ==================================
typedef void (*SAMPLELOADCALLBACK)(short *buff, size_t length_samples, int instno, int sampno);
typedef void (*FMUSIC_CALLBACK)(FMUSIC_MODULE *mod, unsigned char param);

// this must be called before FSOUND_Init!
void FSOUND_File_SetCallbacks(void*         (*OpenCallback)(const char *name),
                              void			(*CloseCallback)(void* handle),
                              int			(*ReadCallback)(void *buffer, int size, void* handle),
                              void			(*SeekCallback)(void* handle, int pos, int mode),
                              int			(*TellCallback)(void* handle));

// =============================================================================================
// FMUSIC API
// =============================================================================================

// Song management / playback functions.
// =====================================

FMUSIC_MODULE * FMUSIC_LoadSong(const char *data, SAMPLELOADCALLBACK sampleloadcallback);
bool    		FMUSIC_FreeSong(FMUSIC_MODULE *mod);
bool    		FMUSIC_PlaySong(FMUSIC_MODULE *mod);
void    		FMUSIC_StopSong();

// Runtime song information.
// =========================

int				FMUSIC_GetOrder();
int				FMUSIC_GetRow();
unsigned int	FMUSIC_GetTime();

#endif
