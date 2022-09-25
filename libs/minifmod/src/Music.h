/******************************************************************************/
/* MUSIC.H                                                                    */
/* ----------------                                                           */
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/

#ifndef MUSIC_H_
#define MUSIC_H_

struct FMUSIC_TIMMEINFO
{
	unsigned char order;
	unsigned char row;
	unsigned int  samples;
};

//= VARIABLE EXTERNS ========================================================================
inline FMUSIC_TIMMEINFO *		FMUSIC_TimeInfo = nullptr;

#endif
