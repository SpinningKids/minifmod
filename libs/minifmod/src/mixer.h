
#pragma once

#include <functional>
#include <thread>

#include "position.h"
#include "sample.h"

#ifdef WIN32
#define NOMINMAX
#include <Windows.h>
#endif

struct TimeInfo
{
	Position position;
	unsigned int samples;
};

enum class MixDir
{
	Forwards,
	Backwards
};

struct MixerChannel
{
	unsigned int 	sampleoffset; 	// sample offset (sample starts playing from here).

	const Sample* sptr;			// currently playing sample

	// software mixer stuff
	float			leftvolume;     // mixing information. adjusted volume for left channel (panning involved)
	float			rightvolume;    // mixing information. adjusted volume for right channel (panning involved)
	float			mixpos;			// mixing information. floating point fractional position in sample.
	float			speed;			// mixing information. playback rate - floating point.
	MixDir			speeddir;		// mixing information. playback direction - forwards or backwards

	// software mixer volume ramping stuff
	float			filtered_leftvolume;
	float			filtered_rightvolume;
};

class Mixer final
{
	// mixing info
	std::function<Position()>	tick_callback_;
	unsigned int				mix_rate_ = 44100;		// mixing rate in hz.
	unsigned int				block_size_;			// LATENCY ms worth of samples
	unsigned int				total_blocks_;
	unsigned int				buffer_size_;			// size of 1 'latency' ms buffer in bytes
	float						volume_filter_k_;
	std::unique_ptr<TimeInfo[]> time_info_;

	unsigned int				software_fill_block_ = 0;

	std::unique_ptr<float[]>	mix_buffer_;			// mix output buffer (stereo 32bit float)

	//= VARIABLE EXTERNS ==========================================================================
	MixerChannel				channel_[64];			// channel pool

	// thread control variables
	unsigned int				software_real_block_ = 0;
	unsigned int				mixer_samples_left_;
	unsigned int				bpm_;
	unsigned int				samples_mixed_;		// time passed in samples since song started
	Position					last_position_;

	struct FSOUND_SoundBlock
	{
		std::unique_ptr<short[]> data;
#ifdef WIN32
		WAVEHDR		wavehdr{};
#endif
	};

	FSOUND_SoundBlock			mix_block_;

#ifdef WIN32
	HWAVEOUT					wave_out_handle_ = nullptr;
#endif

	std::thread					software_thread_;
	bool						software_thread_exit_ = false;		// mixing thread termination flag

	void double_buffer_thread() noexcept;
	void fill() noexcept;
public:
	explicit Mixer(std::function<Position()>&& tick_callback, uint16_t bpm, unsigned int mixrate = 44100) noexcept;

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

	[[nodiscard]] unsigned int getMixRate() const
	{
		return mix_rate_;
	}

	void mix(float* mixptr, unsigned int len) noexcept;

	[[nodiscard]] float timeFromSamples() const noexcept;
	[[nodiscard]] MixerChannel& getChannel(int index) noexcept { return channel_[index]; }
	[[nodiscard]] const MixerChannel& getChannel(int index) const { return channel_[index]; }
	void setBPM(unsigned int bpm)
	{
		bpm_ = bpm;
	}
};
