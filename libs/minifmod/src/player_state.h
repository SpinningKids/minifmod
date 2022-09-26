
#pragma once

#include <algorithm>

#include "module.h"
#include "mixer.h"
#include "position.h"

// Song type - contains info on song
struct PlayerState final
{
private:
	std::unique_ptr<Module> module_;
	Mixer mixer_;
public:
	int			global_volume_;		// global mod volume
	int			global_volume_slide_;			// global mod volume
	int			tick_;				// current mod tick
	int			ticks_per_row_;		// speed of song in ticks per row
	int			pattern_delay_;		// pattern delay counter
	Position	current_;
	Position	next_;

    void updateNote() noexcept;
	void updateEffects() noexcept;
	void updateFlags(Channel& channel, const Sample& sample, int globalvolume, bool linearfrequency) noexcept;
public:
	PlayerState(std::unique_ptr<Module> module, int mixrate);

    Position tick() noexcept;

	void setBPM(int bpm) noexcept
	{
		// number of samples
		mixer_.setSamplesPerTick(mixer_.getMixRate() * 5 / (bpm * 2));
	}

	void clampGlobalVolume() noexcept
	{
		global_volume_ = std::clamp(global_volume_, 0, 64);
	}

	std::unique_ptr<Module> stop() noexcept
	{
		mixer_.stop();
		return std::move(module_);
	}

	const Mixer &getMixer() const noexcept
	{
		return mixer_;
	}
};
