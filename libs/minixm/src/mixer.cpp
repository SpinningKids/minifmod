
#include <minixm/mixer.h>

#include <cassert>
#include <algorithm>

#ifdef WIN32
#define NOMINMAX
#include <Windows.h>
#endif

namespace
{
    void MixerClipCopy_Float32(int16_t* dest, const float* src, size_t len) noexcept
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

Mixer::Mixer(std::function<Position()>&& tick_callback, uint16_t bpm, unsigned int mix_rate, unsigned int buffer_size_ms, unsigned int latency, float volume_filter_time_constant) noexcept :
    tick_callback_{std::move(tick_callback)},
    playback_{ mix_rate, buffer_size_ms, latency, std::bind(&Mixer::fill, this, std::placeholders::_1) },
    volume_filter_k_{1.f / (1.f + static_cast<float>(playback_.getMixRate()) * volume_filter_time_constant)},
    mix_buffer_{std::make_unique_for_overwrite<float[]>(static_cast<size_t>(playback_.getBlockSize()) * 2)},
    channel_{},
    mixer_samples_left_{0},
    bpm_{ bpm },
    samples_mixed_{0},
    last_position_{}
{
    for (auto& channel_index : channel_)
    {
        channel_index.speed = 1.0f;
    }
}

void Mixer::mix(float* mixptr, unsigned int len) noexcept
{
    //==============================================================================================
    // LOOP THROUGH CHANNELS
    //==============================================================================================
    for (auto& channel : channel_)
    {
        unsigned int sample_index = 0;

        //==============================================================================================
        // LOOP THROUGH CHANNELS
        //==============================================================================================
        while (channel.sample_ptr && len > sample_index)
        {
            const auto loop_end = static_cast<float>(channel.sample_ptr->header.loop_start + channel.sample_ptr->header.loop_length);

            float samples_to_mix; // This can occasionally be < 0
            if (channel.speed_direction == MixDir::Forwards)
            {
                samples_to_mix = loop_end - channel.mix_position;
                if (samples_to_mix <= 0)
                {
                    samples_to_mix = static_cast<float>(channel.sample_ptr->header.length) - channel.mix_position;
                }
            }
            else
            {
                samples_to_mix = channel.mix_position - static_cast<float>(channel.sample_ptr->header.loop_start);
            }

            // Ensure that we don't try to mix a negative amount of samples
            const auto samples_to_mix_target = static_cast<unsigned int>(std::max(0.f, ceil(samples_to_mix / fabs(channel.speed)))); // round up the division

            // =========================================================================================
            // the following code sets up a mix counter. it sees what will happen first, will the output buffer
            // end be reached first or will the end of the sample be reached first?
            // whatever is smallest will be the mixcount.
            const unsigned int mix_count = std::min(len - sample_index, samples_to_mix_target);

            float speed = channel.speed;

            if (channel.speed_direction != MixDir::Forwards)
            {
                speed = -speed;
            }

            //= SET UP VOLUME MULTIPLIERS ==================================================

            for (unsigned int i = 0; i < mix_count; ++i)
            {
                const auto mixpos = static_cast<uint32_t>(channel.mix_position);
                const float frac = channel.mix_position - static_cast<float>(mixpos);
                const auto samp1 = channel.sample_ptr->buff[mixpos];
                const float newsamp = static_cast<float>(channel.sample_ptr->buff[mixpos + 1] - samp1) * frac + static_cast<float>(samp1);
                mixptr[0 + (sample_index + i) * 2] += channel.filtered_left_volume * newsamp;
                mixptr[1 + (sample_index + i) * 2] += channel.filtered_right_volume * newsamp;
                channel.filtered_left_volume += (channel.left_volume - channel.filtered_left_volume) * volume_filter_k_;
                channel.filtered_right_volume += (channel.right_volume - channel.filtered_right_volume) * volume_filter_k_;
                channel.mix_position += speed;
            }

            sample_index += mix_count;

            //=============================================================================================
            // SWITCH ON LOOP MODE TYPE
            //=============================================================================================
            if (mix_count == samples_to_mix_target)
            {
                if (channel.sample_ptr->header.loop_mode == XMLoopMode::Normal)
                {
                    do
                    {
                        channel.mix_position -= static_cast<float>(channel.sample_ptr->header.loop_length);
                    } while (channel.mix_position >= loop_end);
                }
                else if (channel.sample_ptr->header.loop_mode == XMLoopMode::Bidi)
                {
                    do {
                        if (channel.speed_direction != MixDir::Forwards)
                        {
                            //BidiBackwards
                            channel.mix_position = static_cast<float>(2 * channel.sample_ptr->header.loop_start) - channel.mix_position - 1;
                            channel.speed_direction = MixDir::Forwards;
                            if (channel.mix_position < loop_end)
                            {
                                break;
                            }
                        }
                        //BidiForward
                        channel.mix_position = 2 * loop_end - channel.mix_position - 1;
                        channel.speed_direction = MixDir::Backwards;

                    } while (channel.mix_position < static_cast<float>(channel.sample_ptr->header.loop_start));
                }
                else
                {
                    channel.mix_position = 0;
                    channel.sample_ptr = nullptr;
                }
            }
        }
    }
}

TimeInfo Mixer::fill(short target[]) noexcept
{
    unsigned int block_size = playback_.getBlockSize();

    //==============================================================================
    // MIXBUFFER CLEAR
    //==============================================================================

    memset(mix_buffer_.get(), 0, block_size * sizeof(float) * 2);

    //==============================================================================
    // UPDATE MUSIC
    //==============================================================================

    unsigned int MixedSoFar = 0;

    // keep resetting the mix pointer to the beginning of this portion of the ring buffer
    float* MixPtr = mix_buffer_.get();

    while (MixedSoFar < block_size)
    {
        if (!mixer_samples_left_)
        {
            last_position_ = tick_callback_();	// update new mod tick
            mixer_samples_left_ = playback_.getMixRate() * 5 / (bpm_ * 2);
        }

        const unsigned int SamplesToMix = std::min(mixer_samples_left_, block_size - MixedSoFar);

        mix(MixPtr, SamplesToMix);

        MixedSoFar += SamplesToMix;
        MixPtr += static_cast<size_t>(SamplesToMix) * 2;
        mixer_samples_left_ -= SamplesToMix;

    }

    samples_mixed_ += MixedSoFar; // This is (and was before) approximated down by as much as 1ms per block

    // ====================================================================================
    // CLIP AND COPY BLOCK TO OUTPUT BUFFER
    // ====================================================================================
    MixerClipCopy_Float32(target, mix_buffer_.get(), block_size);

    return { last_position_, samples_mixed_ };
}
