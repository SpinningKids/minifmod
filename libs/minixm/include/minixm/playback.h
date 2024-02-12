
#pragma once

#include <functional>
#include <memory>
#include <thread>

#include "position.h"

#ifdef WIN32
#define NOMINMAX
#include <Windows.h>
#endif

struct TimeInfo final
{
	Position position;
	unsigned int samples;
};

class Playback
{
	struct FSOUND_SoundBlock
	{
		std::unique_ptr<short[]> data;
#ifdef WIN32
		WAVEHDR		wavehdr{};
#endif
	};

	unsigned int				mix_rate_;				// mixing rate in hz.
	unsigned int				block_size_;			// LATENCY ms worth of samples
	unsigned int				total_blocks_;
	unsigned int				buffer_size_;			// size of 1 'latency' ms buffer in bytes

	FSOUND_SoundBlock			mix_block_;

#ifdef WIN32
	HWAVEOUT					wave_out_handle_ = nullptr;
#endif

	std::thread					software_thread_;
	bool						software_thread_exit_ = true;		// mixing thread termination flag

	unsigned int				software_fill_block_ = 0;
	unsigned int				software_real_block_ = 0;

	std::unique_ptr<TimeInfo[]> time_info_;

	std::function<TimeInfo(short[])> do_fill_;

	void double_buffer_thread() noexcept;
	void fill() noexcept;
public:
	explicit Playback(unsigned int mix_rate, unsigned int buffer_size_ms, unsigned int latency, std::function<TimeInfo(short[])>&& do_fill) :
		mix_rate_{ mix_rate },
		block_size_{ (((mix_rate_ * latency / 1000) + 3) & 0xFFFFFFFC) },
		total_blocks_{ (buffer_size_ms / latency) * 2 },
		buffer_size_{ block_size_ * total_blocks_ },
		mix_block_{ std::make_unique_for_overwrite<short[]>(static_cast<size_t>(buffer_size_) * 2) },
		time_info_{ new TimeInfo[total_blocks_] },
		do_fill_{ std::move(do_fill) }
	{}

	void start()
	{
		if (software_thread_exit_) {
			software_thread_exit_ = false;
			software_thread_ = std::thread{ &Playback::double_buffer_thread, this };
		}
	}

	[[nodiscard]] float timeFromSamples() const noexcept;

	[[nodiscard]] unsigned int getMixRate() const noexcept
	{
		return mix_rate_;
	}

	[[nodiscard]] unsigned int getTotalBlocks() const noexcept
	{
		return total_blocks_;
	}

	[[nodiscard]] unsigned int getBlockSize() const noexcept
	{
		return block_size_;
	}

	[[nodiscard]] const TimeInfo& getTimeInfo() const noexcept
	{
		return time_info_[software_real_block_];
	}

	void stop()
	{
		// wait until callback has settled down and exited
		if (software_thread_.joinable())
		{
			// Kill the thread
			software_thread_exit_ = true;
			software_thread_.join();
		}
	}
};
