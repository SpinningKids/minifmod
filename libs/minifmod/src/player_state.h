
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
	Channel FMUSIC_Channel[32]{};		// channel array for this song

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

	void updateNote() noexcept;
	void updateTick() noexcept;

	void clampGlobalVolume() noexcept
	{
		global_volume_ = std::clamp(global_volume_, 0, 64);
	}

	Position tick() noexcept;
public:
	PlayerState(std::unique_ptr<Module> module, unsigned int mixrate);

	void setBPM(unsigned int bpm) noexcept
	{
		mixer_.setBPM(bpm);
	}

	std::unique_ptr<Module> stop() noexcept
	{
		mixer_.stop();
		return std::move(module_);
	}

	[[nodiscard]] const Mixer& getMixer() const noexcept
	{
		return mixer_;
	}
};
