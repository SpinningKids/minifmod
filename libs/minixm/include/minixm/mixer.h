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

#include "mixer_channel.h"
#include "playback.h"
#include "position.h"
#include "sample.h"

struct TimeInfo final
{
    Position position;
    uint32_t samples;
};

class Mixer final
{
public:
    using TickFunction = Position(void* context);

private:
    // mixing info
    TickFunction *tick_function_;
    void* tick_context_;

    std::unique_ptr<IPlaybackDriver> driver_;
    std::unique_ptr<TimeInfo[]> time_info_;

    float volume_filter_k_;
    std::unique_ptr<float[]> mix_buffer_; // mix output buffer (stereo 32bit float)

    //= VARIABLE EXTERNS ==========================================================================
    MixerChannel channel_[64]; // channel pool

    // thread control variables
    uint32_t mixer_samples_left_;
    unsigned int bpm_;
    TimeInfo last_mixed_time_info_;

    const TimeInfo& fill(short target[]) noexcept;

public:
    explicit Mixer(
        TickFunction tick_function,
        void* tick_context,
        uint16_t bpm,
        unsigned int mix_rate = 44100,
        unsigned int buffer_size_ms = 1000,
        unsigned int latency = 20,
        float volume_filter_time_constant = 0.003f);

    [[nodiscard]] MixerChannel& getChannel(int index)
    {
        assert(index >= 0 && static_cast<size_t>(index) < std::size(channel_));
        return channel_[index];
    }

    [[nodiscard]] const MixerChannel& getChannel(int index) const
    {
        assert(index >= 0 && static_cast<size_t>(index) < std::size(channel_));
        return channel_[index];
    }

    void setBPM(unsigned int bpm) noexcept { bpm_ = bpm; }

    [[nodiscard]] unsigned int getMixRate() const noexcept;
    [[nodiscard]] TimeInfo getTimeInfo() const;

    void start();
    void stop();
    [[nodiscard]] float timeFromSamples() const;
};
