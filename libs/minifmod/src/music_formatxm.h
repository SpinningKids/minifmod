/******************************************************************************/
/* FORMATXM.H                                                                 */
/* ----------------                                                           */
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/

#ifndef FMUSIC_FORMATXM_
#define FMUSIC_FORMATXM_

#include "Music.h"

#include <memory>

enum FMUSIC_XMCOMMANDS
{
	FMUSIC_XM_ARPEGGIO,
	FMUSIC_XM_PORTAUP,
	FMUSIC_XM_PORTADOWN,
	FMUSIC_XM_PORTATO,
	FMUSIC_XM_VIBRATO,
	FMUSIC_XM_PORTATOVOLSLIDE,
	FMUSIC_XM_VIBRATOVOLSLIDE,
	FMUSIC_XM_TREMOLO,
	FMUSIC_XM_SETPANPOSITION,
	FMUSIC_XM_SETSAMPLEOFFSET,
	FMUSIC_XM_VOLUMESLIDE,
	FMUSIC_XM_PATTERNJUMP,
	FMUSIC_XM_SETVOLUME,
	FMUSIC_XM_PATTERNBREAK,
	FMUSIC_XM_SPECIAL,
	FMUSIC_XM_SETSPEED,
	FMUSIC_XM_SETGLOBALVOLUME,
	FMUSIC_XM_GLOBALVOLSLIDE,
	FMUSIC_XM_I,
	FMUSIC_XM_J,
	FMUSIC_XM_KEYOFF,
	FMUSIC_XM_SETENVELOPEPOS,
	FMUSIC_XM_M,
	FMUSIC_XM_N,
	FMUSIC_XM_O,
	FMUSIC_XM_PANSLIDE,
	FMUSIC_XM_Q,
	FMUSIC_XM_MULTIRETRIG,
	FMUSIC_XM_S,
	FMUSIC_XM_TREMOR,
	FMUSIC_XM_U,
	FMUSIC_XM_V,
	FMUSIC_XM_W,
	FMUSIC_XM_EXTRAFINEPORTA,
	FMUSIC_XM_Y,
	FMUSIC_XM_Z,
};


enum FMUSIC_XMCOMMANDSSPECIAL
{
	FMUSIC_XM_SETFILTER,
	FMUSIC_XM_FINEPORTAUP,
	FMUSIC_XM_FINEPORTADOWN,
	FMUSIC_XM_SETGLISSANDO,
	FMUSIC_XM_SETVIBRATOWAVE,
	FMUSIC_XM_SETFINETUNE,
	FMUSIC_XM_PATTERNLOOP,
	FMUSIC_XM_SETTREMOLOWAVE,
	FMUSIC_XM_SETPANPOSITION16,
	FMUSIC_XM_RETRIG,
	FMUSIC_XM_FINEVOLUMESLIDEUP,
	FMUSIC_XM_FINEVOLUMESLIDEDOWN,
	FMUSIC_XM_NOTECUT,
	FMUSIC_XM_NOTEDELAY,
	FMUSIC_XM_PATTERNDELAY,
	FMUSIC_XM_FUNKREPEAT,
};

#define FMUSIC_XMFLAGS_LINEARFREQUENCY		1

std::unique_ptr<FMUSIC_MODULE> XMLoad(void *fp, SAMPLELOADCALLBACK sampleloadcallback);
void XMTick(FMUSIC_MODULE& mod) noexcept;

#endif
