
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
    mix_rate_{mix_rate},
    block_size_{(((mix_rate_ * latency / 1000) + 3) & 0xFFFFFFFC)},
    total_blocks_{(buffer_size_ms / latency) * 2},
    buffer_size_{block_size_ * total_blocks_},
    volume_filter_k_{1.f / (1.f + static_cast<float>(mix_rate_) * volume_filter_time_constant)},
    time_info_{new TimeInfo[total_blocks_]},
    mix_buffer_{std::make_unique_for_overwrite<float[]>(static_cast<size_t>(block_size_) * 2)},
    channel_{},
    mixer_samples_left_{0},
    bpm_{ bpm },
    samples_mixed_{0},
    last_position_{},
    mix_block_{std::make_unique_for_overwrite<short[]>(static_cast<size_t>(buffer_size_) * 2)},
    software_thread_{&Mixer::double_buffer_thread, this}
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

float Mixer::timeFromSamples() const noexcept
{
#ifdef WIN32
    if (wave_out_handle_) {
        MMTIME mmtime;
        mmtime.wType = TIME_SAMPLES;
        waveOutGetPosition(wave_out_handle_, &mmtime, sizeof(mmtime));
        return static_cast<float>(mmtime.u.sample) / static_cast<float>(mix_rate_);
    }
#endif
    return 0;
}

void Mixer::double_buffer_thread() noexcept
{
    software_thread_exit_ = false;

    {

#ifdef WIN32
        // ========================================================================================================
        // INITIALIZE WAVEOUT
        // ========================================================================================================
        WAVEFORMATEX	pcmwf;
        pcmwf.wFormatTag = WAVE_FORMAT_PCM;
        pcmwf.nChannels = 2;
        pcmwf.wBitsPerSample = 16;
        pcmwf.nBlockAlign = pcmwf.nChannels * pcmwf.wBitsPerSample / 8;
        pcmwf.nSamplesPerSec = mix_rate_;
        pcmwf.nAvgBytesPerSec = pcmwf.nSamplesPerSec * pcmwf.nBlockAlign;
        pcmwf.cbSize = 0;

        if (waveOutOpen(&wave_out_handle_, WAVE_MAPPER, &pcmwf, 0, 0, 0))
        {
            software_thread_exit_ = true;
            return;
        }
#endif
    }
    // ========================================================================================================
    // PREFILL THE MIXER BUFFER
    // ========================================================================================================

#ifdef WIN32
    mix_block_.wavehdr.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
    mix_block_.wavehdr.lpData = reinterpret_cast<LPSTR>(mix_block_.data.get());
    mix_block_.wavehdr.dwBufferLength = buffer_size_ * sizeof(short) * 2;
    mix_block_.wavehdr.dwBytesRecorded = 0;
    mix_block_.wavehdr.dwUser = 0;
    mix_block_.wavehdr.dwLoops = -1;
    waveOutPrepareHeader(wave_out_handle_, &mix_block_.wavehdr, sizeof(WAVEHDR));
#endif

    do
    {
        fill();
    } while (software_fill_block_);

    // ========================================================================================================
    // START THE OUTPUT
    // ========================================================================================================

#ifdef WIN32
    waveOutWrite(wave_out_handle_, &mix_block_.wavehdr, sizeof(WAVEHDR));
#endif

    while (!software_thread_exit_)
    {
#ifdef WIN32
        MMTIME	mmt{
            .wType = TIME_BYTES
        };

        waveOutGetPosition(wave_out_handle_, &mmt, sizeof(MMTIME));

        const unsigned int cursorpos = (mmt.u.cb / 4) % buffer_size_;
#else
        const unsigned int cursorpos = 0;
#endif
        const unsigned int cursorblock = cursorpos / block_size_;

        while (software_fill_block_ != cursorblock)
        {
            fill();

            ++software_real_block_;
            if (software_real_block_ >= total_blocks_)
            {
                software_real_block_ = 0;
            }
        }

#ifdef WIN32
        Sleep(5);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
#endif
    }

#ifdef WIN32
    if (mix_block_.wavehdr.lpData)
    {
        waveOutUnprepareHeader(wave_out_handle_, &mix_block_.wavehdr, sizeof(WAVEHDR));
        mix_block_.wavehdr.dwFlags &= ~WHDR_PREPARED;
        mix_block_.wavehdr.lpData = nullptr;
    }
#endif

    // ========================================================================================================
    // SHUT DOWN OUTPUT DRIVER
    // ========================================================================================================
#ifdef WIN32
    waveOutReset(wave_out_handle_);

    waveOutClose(wave_out_handle_);
    wave_out_handle_ = nullptr;
#endif

}

void Mixer::fill() noexcept
{
    const unsigned int mixpos = software_fill_block_ * block_size_;

    //==============================================================================
    // MIXBUFFER CLEAR
    //==============================================================================

    memset(mix_buffer_.get(), 0, block_size_ * sizeof(float) * 2);

    //==============================================================================
    // UPDATE MUSIC
    //==============================================================================

    {
        unsigned int MixedSoFar = 0;

        // keep resetting the mix pointer to the beginning of this portion of the ring buffer
        float* MixPtr = mix_buffer_.get();

        while (MixedSoFar < block_size_)
        {
            if (!mixer_samples_left_)
            {
                last_position_ = tick_callback_();	// update new mod tick
                mixer_samples_left_ = mix_rate_ * 5 / (bpm_ * 2);
            }

            const unsigned int SamplesToMix = std::min(mixer_samples_left_, block_size_ - MixedSoFar);

            mix(MixPtr, SamplesToMix);

            MixedSoFar += SamplesToMix;
            MixPtr += static_cast<size_t>(SamplesToMix) * 2;
            mixer_samples_left_ -= SamplesToMix;

        }

        samples_mixed_ += MixedSoFar; // This is (and was before) approximated down by as much as 1ms per block

        time_info_[software_fill_block_].samples = samples_mixed_;
        time_info_[software_fill_block_].position = last_position_;
    }


    // ====================================================================================
    // CLIP AND COPY BLOCK TO OUTPUT BUFFER
    // ====================================================================================
    MixerClipCopy_Float32(mix_block_.data.get() + static_cast<size_t>(mixpos) * 2, mix_buffer_.get(), block_size_);

    ++software_fill_block_;

    if (software_fill_block_ >= total_blocks_)
    {
        software_fill_block_ = 0;
    }
}
