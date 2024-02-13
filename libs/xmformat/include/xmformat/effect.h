
#pragma once

#include <cstdint>

#pragma pack(push)
#pragma pack(1)

enum class XMEffect : uint8_t
{
	ARPEGGIO,
	PORTAUP,
	PORTADOWN,
	PORTATO,
	VIBRATO,
	PORTATOVOLSLIDE,
	VIBRATOVOLSLIDE,
	TREMOLO,
	SETPANPOSITION,
	SETSAMPLEOFFSET,
	VOLUMESLIDE,
	PATTERNJUMP,
	SETVOLUME,
	PATTERNBREAK,
	SPECIAL,
	SETSPEED,
	SETGLOBALVOLUME,
	GLOBALVOLSLIDE,
	I,
	J,
	KEYOFF,
	SETENVELOPEPOS,
	M,
	N,
	O,
	PANSLIDE,
	Q,
	MULTIRETRIG,
	S,
	TREMOR,
	U,
	V,
	W,
	EXTRAFINEPORTA,
	Y,
	Z,
};

static_assert(sizeof(XMEffect) == 1);

enum class XMSpecialEffect : uint8_t
{
	SETFILTER,
	FINEPORTAUP,
	FINEPORTADOWN,
	SETGLISSANDO,
	SETVIBRATOWAVE,
	SETFINETUNE,
	PATTERNLOOP,
	SETTREMOLOWAVE,
	SETPANPOSITION16,
	RETRIG,
	FINEVOLUMESLIDEUP,
	FINEVOLUMESLIDEDOWN,
	NOTECUT,
	NOTEDELAY,
	PATTERNDELAY,
	FUNKREPEAT,
};

static_assert(sizeof(XMSpecialEffect) == 1);

#pragma pack(pop)
