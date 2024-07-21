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

#include <minixm/mixer.h>

#include <cassert>
#include <algorithm>

namespace
{
    void MixerClipCopy_Float32(int16_t* dest, const float* src, size_t len)
    {
        assert(src);
        assert(dest);
        for (size_t i = 0; i < len * 2; i++)
        {
            *dest++ = static_cast<int16_t>(std::clamp(
                static_cast<int>(*src++),
                static_cast<int>(std::numeric_limits<int16_t>::min()),
                static_cast<int>(std::numeric_limits<int16_t>::max())));
        }
    }
}

Mixer::Mixer(std::unique_ptr<IPlaybackDriver> driver, TickFunction* tick_function, void* tick_context, uint16_t bpm,
             float volume_filter_time_constant) :
    tick_function_{tick_function},
    tick_context_{tick_context},
    driver_{std::move(driver)},
    time_info_{std::make_unique_for_overwrite<TimeInfo[]>(driver_->blocks())},
    volume_filter_k_{1.f / (1.f + static_cast<float>(driver_->mix_rate()) * volume_filter_time_constant)},
    mix_buffer_{std::make_unique_for_overwrite<float[]>(driver_->block_size() * 2)},
    channel_{},
    mixer_samples_left_{0},
    bpm_{bpm},
    last_mixed_time_info_{}
{
    for (auto& channel : channel_)
    {
        channel.speed = 1.0f;
    }
}

unsigned Mixer::getMixRate() const noexcept
{
    return driver_->mix_rate();
}

TimeInfo Mixer::getTimeInfo() const
{
    return time_info_[driver_->current_block_played()];
}

void Mixer::start()
{
    driver_->start([](void* arg, size_t block, short data[]) noexcept
    {
        const auto self = static_cast<Mixer*>(arg);
        self->time_info_[block] = self->fill(data);
    }, this);
}

void Mixer::stop()
{
    driver_->stop();
}

float Mixer::timeFromSamples() const
{
    return static_cast<float>(static_cast<double>(time_info_[driver_->current_block_played()].samples) / driver_->
        mix_rate());
}

const TimeInfo& Mixer::fill(short target[]) noexcept
{
    const auto block_size = driver_->block_size();
    //==============================================================================
    // MIXBUFFER CLEAR
    //==============================================================================

    memset(mix_buffer_.get(), 0, block_size * sizeof(float) * 2);

    //==============================================================================
    // UPDATE MUSIC
    //==============================================================================

    uint32_t MixedSoFar = 0;

    // keep resetting the mix pointer to the beginning of this portion of the ring buffer
    float* MixPtr = mix_buffer_.get();

    while (MixedSoFar < block_size)
    {
        if (!mixer_samples_left_)
        {
            last_mixed_time_info_.position = tick_function_(tick_context_); // update new mod tick
            mixer_samples_left_ = driver_->mix_rate() * 5 / (bpm_ * 2);
        }

        const uint32_t SamplesToMix = std::min(mixer_samples_left_, block_size - MixedSoFar);

        //==============================================================================================
        // LOOP THROUGH CHANNELS
        //==============================================================================================
        for (auto& channel : channel_)
        {
            channel.mix(MixPtr, SamplesToMix, volume_filter_k_);
        }

        MixedSoFar += SamplesToMix;
        MixPtr += static_cast<size_t>(SamplesToMix) * 2;
        mixer_samples_left_ -= SamplesToMix;
    }

    last_mixed_time_info_.samples += MixedSoFar;
    // This is (and was before) approximated down by as much as 1ms per block

    // ====================================================================================
    // CLIP AND COPY BLOCK TO OUTPUT BUFFER
    // ====================================================================================
    MixerClipCopy_Float32(target, mix_buffer_.get(), block_size);

    return last_mixed_time_info_;
}
