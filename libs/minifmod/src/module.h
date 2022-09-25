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

#include <algorithm>
#include <cstdint>

#include <minifmod/minifmod.h>

#include "channel.h"
#include "instrument.h"
#include "pattern.h"
#include "system_file.h"
#include "xmfile.h"
#include "Sound.h"

inline FMUSIC_CHANNEL			FMUSIC_Channel[32]{};		// channel array for this song

// Song type - contains info on song
struct FMUSIC_MODULE final
{
	XMHeader	header_;
	Pattern		pattern_[256];		// patterns array for this song
	Instrument	instrument_[128];	// instrument array for this song (not used in MOD/S3M)
	int			mixer_samples_left_;
	int			mixer_samples_per_tick_;

	int			global_volume_;		// global mod volume
	int			globalvsl;			// global mod volume
	int			tick_;				// current mod tick
	int			ticks_per_row_;		// speed of song in ticks per row
	uint8_t		row_;				// current row in pattern
	uint8_t		order_;				// current song order position
	int			pattern_delay_;		// pattern delay counter
	uint8_t		next_row_;			// current row in pattern
	uint8_t		next_order_;		// current song order position
	int			samples_mixed_;		// time passed in seconds since song started


	FMUSIC_MODULE(const minifmod::FileAccess& fileAccess, void* fp, SAMPLELOADCALLBACK sampleloadcallback);
	void tick() noexcept;

	void setBPM(int bpm) noexcept
	{
		// number of samples
		mixer_samples_per_tick_ = FSOUND_MixRate * 5 / (bpm * 2);
	}

	void clampGlobalVolume() noexcept
	{
		global_volume_ = std::clamp(global_volume_, 0, 64);
	}

	void reset() noexcept;
private:
	void updateNote() noexcept;
	void updateEffects() noexcept;
};
