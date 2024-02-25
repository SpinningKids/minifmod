
#pragma once

#include <algorithm>

#include "module.h"
#include "mixer.h"
#include "position.h"
#include "xmeffects.h"

// Song type - contains info on song
struct PlayerState final
{
private:
	Channel			channels_[32]{};	// channel array for this song

	std::unique_ptr<Module> module_;
	Mixer mixer_;
	int				global_volume_;		// global mod volume
	int				tick_;				// current mod tick
	int				ticks_per_row_;		// speed of song in ticks per row
	int				pattern_delay_;		// pattern delay counter
	Position		current_;
	Position		next_;
#ifdef FMUSIC_XM_GLOBALVOLSLIDE_ACTIVE
	int				global_volume_slide_ = 0;			// global mod volume
#endif

	void updateNote();
	void updateTick();

	void clampGlobalVolume() noexcept
	{
		global_volume_ = std::clamp(global_volume_, 0, 64);
	}

	Position tick();
public:
	PlayerState(std::unique_ptr<Module> module, unsigned int mix_rate);

	void start()
	{
	    mixer_.start();
	}

	void setBPM(unsigned int bpm) noexcept
	{
	    mixer_.setBPM(bpm);
	}

	std::unique_ptr<Module> stop()
	{
		mixer_.stop();
		return std::move(module_);
	}

    TimeInfo getTimeInfo() const
	{
	    return mixer_.getTimeInfo();
	}
};
