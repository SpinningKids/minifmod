/******************************************************************************/
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/
/* This library (xmformat) is maintained by Pan/SpinningKids, 2022-2024       */
/******************************************************************************/

#pragma once

#include "effect.h"
#include "note.h"

#pragma pack(push)
#pragma pack(1)

struct XMPatternCell
{
	XMNote note;
	uint8_t instrument_number;	// sample being played 0 = use last, 1-128 instrument number
	uint8_t volume;				// volume column value (0-64)  255=no volume
	XMEffect effect;			// effect number       (0-1Ah)
	uint8_t effect_parameter;	// effect parameter    (0-255)
};

static_assert(sizeof(XMPatternCell) == 5);

#pragma pack(pop)
