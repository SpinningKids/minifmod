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

#pragma once

#include <cassert>
#include <cstdint>

#include <minifmod/minifmod.h>

#include "channel.h"
#include "instrument.h"
#include "pattern.h"
#include "system_file.h"
#include "xmfile.h"

inline Channel			FMUSIC_Channel[32]{};		// channel array for this song

struct Module final
{
	XMHeader	header_;
	Pattern		pattern_[256];		// patterns array for this song
	Instrument	instrument_[128];	// instrument array for this song (not used in MOD/S3M)

	Module(const minifmod::FileAccess& fileAccess, void* fp, SAMPLELOADCALLBACK sampleloadcallback);

	const Instrument& getInstrument(int instrument) const noexcept
	{
		assert(instrument < (int)header_.instruments_count);
		return instrument_[instrument];
	}
};