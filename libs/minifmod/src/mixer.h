
#pragma once

#include <functional>
#include <thread>

#include "position.h"
#include "sample.h"

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
	unsigned int				FSOUND_MixRate = 44100;		// mixing rate in hz.
	unsigned int				FSOUND_BlockSize;			// LATENCY ms worth of samples
	unsigned int				totalblocks;
	unsigned int				FSOUND_BufferSize;			// size of 1 'latency' ms buffer in bytes
	float						volume_filter_k;
	std::unique_ptr<TimeInfo[]> FMUSIC_TimeInfo;

	unsigned int				FSOUND_Software_FillBlock = 0;

	std::unique_ptr<float[]>	FSOUND_MixBuffer;			// mix output buffer (stereo 32bit float)

	//= VARIABLE EXTERNS ==========================================================================
	MixerChannel				FSOUND_Channel[64];			// channel pool

	// thread control variables
	std::thread					Software_Thread;
	bool						Software_Thread_Exit = false;		// mixing thread termination flag
	unsigned int				FSOUND_Software_RealBlock = 0;
	unsigned int				mixer_samples_left_;
	unsigned int				mixer_samples_per_tick_;
	unsigned int				samples_mixed_;		// time passed in samples since song started
	Position					last_position_;

	void double_buffer_thread() noexcept;
	void fill() noexcept;
public:
	explicit Mixer(std::function<Position()>&& tick_callback, unsigned int mixrate = 44100) noexcept;

	[[nodiscard]] const TimeInfo& getTimeInfo() const noexcept
	{
		return FMUSIC_TimeInfo[FSOUND_Software_RealBlock];
	}

	void stop()
	{
		// wait until callback has settled down and exited
		if (Software_Thread.joinable())
		{
			// Kill the thread
			Software_Thread_Exit = true;
			Software_Thread.join();
		}
	}

	[[nodiscard]] unsigned int getMixRate() const
	{
		return FSOUND_MixRate;
	}

	void mix(float* mixptr, unsigned int len) noexcept;

	[[nodiscard]] float timeFromSamples() const noexcept;
	void setSamplesPerTick(unsigned int samplesPerTick)
	{
		mixer_samples_per_tick_ = samplesPerTick;
	}

	[[nodiscard]] MixerChannel& getChannel(int index) noexcept { return FSOUND_Channel[index]; }
	[[nodiscard]] const MixerChannel& getChannel(int index) const { return FSOUND_Channel[index]; }
};
