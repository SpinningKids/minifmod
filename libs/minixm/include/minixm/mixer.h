/******************************************************************************/
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/
/* This library (minixm) is maintained by Pan/SpinningKids, 2022-2024         */
/******************************************************************************/

#pragma once

#include <cassert>
#include <functional>

#include "playback.h"
#include "position.h"
#include "sample.h"

enum class MixDir
{
    Forwards,
    Backwards
};

struct MixerChannel final
{
    unsigned int 	sample_offset;			// sample offset (sample starts playing from here).

    const Sample* 	sample_ptr;				// currently playing sample

    // software mixer stuff
    float			left_volume;			// mixing information. adjusted volume for left channel (panning involved)
    float			right_volume;			// mixing information. adjusted volume for right channel (panning involved)
    float			mix_position;			// mixing information. floating point fractional position in sample.
    float			speed;					// mixing information. playback rate - floating point.
    MixDir			speed_direction;		// mixing information. playback direction - forwards or backwards

    // software mixer volume ramping stuff
    float			filtered_left_volume;
    float			filtered_right_volume;
};

struct TimeInfo final
{
    Position position;
    uint32_t samples;
};

class Mixer final
{
    // mixing info
    std::function<Position()>	tick_callback_;

    std::unique_ptr<IPlaybackDriver> driver_;
    std::unique_ptr<TimeInfo[]> time_info_;

    float						volume_filter_k_;
    std::unique_ptr<float[]>	mix_buffer_;			// mix output buffer (stereo 32bit float)

    //= VARIABLE EXTERNS ==========================================================================
    MixerChannel				channel_[64];			// channel pool

    // thread control variables
    uint32_t					mixer_samples_left_;
    unsigned int				bpm_;
    TimeInfo                    last_mixed_time_info_;

    const TimeInfo &fill(short target[]);
    void mix(float* mixptr, uint32_t len);
public:
    explicit Mixer(
        std::function<Position()>&& tick_callback,
        uint16_t bpm,
        unsigned int mix_rate = 44100,
        unsigned int buffer_size_ms = 1000,
        unsigned int latency = 20,
        float volume_filter_time_constant = 0.003f);

    [[nodiscard]] MixerChannel& getChannel(int index)
    {
        assert(index >= 0 && index < std::size(channel_));
        return channel_[index];
    }
    [[nodiscard]] const MixerChannel& getChannel(int index) const
    {
        assert(index >= 0 && index < std::size(channel_));
        return channel_[index];
    }
    void setBPM(unsigned int bpm) noexcept { bpm_ = bpm; }

    [[nodiscard]] unsigned int getMixRate() const noexcept;
    [[nodiscard]] TimeInfo getTimeInfo() const;

    void start();
    void stop();
    [[nodiscard]] float timeFromSamples() const;
};
