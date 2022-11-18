
#include "mixer.h"

#include <cassert>
#include <algorithm>

#ifdef WIN32
#define NOMINMAX
#include <Windows.h>
#endif

namespace
{
    constexpr int	FSOUND_BufferSizeMs = 1000;
    constexpr int	FSOUND_LATENCY = 20;
    constexpr float VolumeFilterTimeConstant = 0.003f; // time constant (RC) of the volume IIR filter

    struct FSOUND_SoundBlock
    {
#ifdef WIN32
        WAVEHDR		wavehdr{};
#endif
        std::unique_ptr<short[]> data;
    };

    FSOUND_SoundBlock	FSOUND_MixBlock;

#ifdef WIN32
    HWAVEOUT			FSOUND_WaveOutHandle = nullptr;
#endif


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

Mixer::Mixer(std::function<Position()>&& tick_callback, int mixrate) noexcept :
    tick_callback_{ std::move(tick_callback) },
    FSOUND_MixRate{ mixrate },
    FSOUND_BlockSize{ static_cast<int>(((FSOUND_MixRate * FSOUND_LATENCY / 1000) + 3) & 0xFFFFFFFC) },
    totalblocks{ (FSOUND_BufferSizeMs / FSOUND_LATENCY) * 2 },
    FSOUND_BufferSize{ FSOUND_BlockSize * totalblocks },
    volume_filter_k{ 1.f / (1.f + static_cast<float>(FSOUND_MixRate) * VolumeFilterTimeConstant) },
    FMUSIC_TimeInfo{ new TimeInfo[totalblocks] },
    FSOUND_Channel{},
    mixer_samples_left_{ 0 },
    samples_mixed_{ 0 },
    last_position_{}
{
    for (auto& channel_index : FSOUND_Channel)
    {
        channel_index.speed = 1.0f;
    }

    FSOUND_MixBlock.data.reset(new short[FSOUND_BufferSize * 2]);

    FSOUND_MixBuffer.reset(new float[FSOUND_BlockSize * 2]);

    Software_Thread = std::thread(&Mixer::double_buffer_thread, this);
}

void Mixer::mix(float* mixptr, int len) noexcept
{
    //==============================================================================================
    // LOOP THROUGH CHANNELS
    //==============================================================================================
    for (auto& channel : FSOUND_Channel)
    {
        int sample_index = 0;

        //==============================================================================================
        // LOOP THROUGH CHANNELS
        //==============================================================================================
        while (channel.sptr && len > sample_index)
        {
            const float loop_end = static_cast<float>(channel.sptr->header.loop_start + channel.sptr->header.loop_length);

            float samples_to_mix; // This can occasionally be < 0
            if (channel.speeddir == MixDir::Forwards)
            {
                samples_to_mix = loop_end - channel.mixpos;
                if (samples_to_mix <= 0)
                {
                    samples_to_mix = static_cast<float>(channel.sptr->header.length) - channel.mixpos;
                }
            }
            else
            {
                samples_to_mix = channel.mixpos - static_cast<float>(channel.sptr->header.loop_start);
            }

            // Ensure that we don't try to mix a negative amount of samples
            const int samples_to_mix_target = std::max(0, static_cast<int>(ceil(samples_to_mix / channel.speed))); // round up the division

            // =========================================================================================
            // the following code sets up a mix counter. it sees what will happen first, will the output buffer
            // end be reached first or will the end of the sample be reached first?
            // whatever is smallest will be the mixcount.
            const int mix_count = std::min(len - sample_index, samples_to_mix_target);
            assert(mix_count >= 0);

            float speed = channel.speed;

            if (channel.speeddir != MixDir::Forwards)
            {
                speed = -speed;
            }

            //= SET UP VOLUME MULTIPLIERS ==================================================

            for (int i = 0; i < mix_count; ++i)
            {
                const uint32_t mixpos = static_cast<uint32_t>(channel.mixpos);
                const float frac = channel.mixpos - static_cast<float>(mixpos);
                const auto samp1 = channel.sptr->buff[mixpos];
                const float newsamp = static_cast<float>(channel.sptr->buff[mixpos + 1] - samp1) * frac + static_cast<float>(samp1);
                mixptr[0 + (sample_index + i) * 2] += channel.filtered_leftvolume * newsamp;
                mixptr[1 + (sample_index + i) * 2] += channel.filtered_rightvolume * newsamp;
                channel.filtered_leftvolume += (channel.leftvolume - channel.filtered_leftvolume) * volume_filter_k;
                channel.filtered_rightvolume += (channel.rightvolume - channel.filtered_rightvolume) * volume_filter_k;
                channel.mixpos += speed;
            }

            sample_index += mix_count;

            //=============================================================================================
            // SWITCH ON LOOP MODE TYPE
            //=============================================================================================
            if (mix_count == samples_to_mix_target)
            {
                if (channel.sptr->header.loop_mode == XMLoopMode::Normal)
                {
                    do
                    {
                        channel.mixpos -= static_cast<float>(channel.sptr->header.loop_length);
                    } while (channel.mixpos >= loop_end);
                }
                else if (channel.sptr->header.loop_mode == XMLoopMode::Bidi)
                {
                    do {
                        if (channel.speeddir != MixDir::Forwards)
                        {
                            //BidiBackwards
                            channel.mixpos = static_cast<float>(2 * channel.sptr->header.loop_start) - channel.mixpos - 1;
                            channel.speeddir = MixDir::Forwards;
                            if (channel.mixpos < loop_end)
                            {
                                break;
                            }
                        }
                        //BidiForward
                        channel.mixpos = 2 * loop_end - channel.mixpos - 1;
                        channel.speeddir = MixDir::Backwards;

                    } while (channel.mixpos < static_cast<float>(channel.sptr->header.loop_start));
                }
                else
                {
                    channel.mixpos = 0;
                    channel.sptr = nullptr;
                }
            }
        }
    }
}

float Mixer::timeFromSamples() const noexcept
{
#ifdef WIN32
    if (FSOUND_WaveOutHandle) {
        MMTIME mmtime;
        mmtime.wType = TIME_SAMPLES;
        waveOutGetPosition(FSOUND_WaveOutHandle, &mmtime, sizeof(mmtime));
        return static_cast<float>(mmtime.u.sample) / static_cast<float>(FSOUND_MixRate);
    }
#endif
    return 0;
}

void Mixer::double_buffer_thread() noexcept
{
    Software_Thread_Exit = false;

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
        pcmwf.nSamplesPerSec = FSOUND_MixRate;
        pcmwf.nAvgBytesPerSec = pcmwf.nSamplesPerSec * pcmwf.nBlockAlign;
        pcmwf.cbSize = 0;

        if (waveOutOpen(&FSOUND_WaveOutHandle, WAVE_MAPPER, &pcmwf, 0, 0, 0))
        {
            Software_Thread_Exit = true;
            return;
        }
#endif
    }
    // ========================================================================================================
    // PREFILL THE MIXER BUFFER
    // ========================================================================================================

#ifdef WIN32
    FSOUND_MixBlock.wavehdr.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
    FSOUND_MixBlock.wavehdr.lpData = reinterpret_cast<LPSTR>(FSOUND_MixBlock.data.get());
    FSOUND_MixBlock.wavehdr.dwBufferLength = FSOUND_BufferSize * 2 * sizeof(short);
    FSOUND_MixBlock.wavehdr.dwBytesRecorded = 0;
    FSOUND_MixBlock.wavehdr.dwUser = 0;
    FSOUND_MixBlock.wavehdr.dwLoops = -1;
    waveOutPrepareHeader(FSOUND_WaveOutHandle, &FSOUND_MixBlock.wavehdr, sizeof(WAVEHDR));
#endif

    do
    {
        fill();
    } while (FSOUND_Software_FillBlock);

    // ========================================================================================================
    // START THE OUTPUT
    // ========================================================================================================

#ifdef WIN32
    waveOutWrite(FSOUND_WaveOutHandle, &FSOUND_MixBlock.wavehdr, sizeof(WAVEHDR));
#endif

    while (!Software_Thread_Exit)
    {
#ifdef WIN32
        MMTIME	mmt{
            .wType = TIME_BYTES
        };

        waveOutGetPosition(FSOUND_WaveOutHandle, &mmt, sizeof(MMTIME));

        const int cursorpos = static_cast<int>(mmt.u.cb / 4) % FSOUND_BufferSize;
#else
        const int cursorpos = 0;
#endif
        const int cursorblock = cursorpos / FSOUND_BlockSize;

        while (FSOUND_Software_FillBlock != cursorblock)
        {
            fill();

            ++FSOUND_Software_RealBlock;
            if (FSOUND_Software_RealBlock >= totalblocks)
            {
                FSOUND_Software_RealBlock = 0;
            }
        }

#ifdef WIN32
        Sleep(5);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
#endif
    }

#ifdef WIN32
    if (FSOUND_MixBlock.wavehdr.lpData)
    {
        waveOutUnprepareHeader(FSOUND_WaveOutHandle, &FSOUND_MixBlock.wavehdr, sizeof(WAVEHDR));
        FSOUND_MixBlock.wavehdr.dwFlags &= ~WHDR_PREPARED;
        FSOUND_MixBlock.wavehdr.lpData = nullptr;
    }
#endif

    // ========================================================================================================
    // SHUT DOWN OUTPUT DRIVER
    // ========================================================================================================
#ifdef WIN32
    waveOutReset(FSOUND_WaveOutHandle);

    waveOutClose(FSOUND_WaveOutHandle);
    FSOUND_WaveOutHandle = nullptr;
#endif

}

void Mixer::fill() noexcept
{
    const int mixpos = FSOUND_Software_FillBlock * FSOUND_BlockSize;

    //==============================================================================
    // MIXBUFFER CLEAR
    //==============================================================================

    memset(FSOUND_MixBuffer.get(), 0, FSOUND_BlockSize * sizeof(float) * 2);

    //==============================================================================
    // UPDATE MUSIC
    //==============================================================================

    {
        int MixedSoFar = 0;

        // keep resetting the mix pointer to the beginning of this portion of the ring buffer
        float* MixPtr = FSOUND_MixBuffer.get();

        while (MixedSoFar < FSOUND_BlockSize)
        {
            if (!mixer_samples_left_)
            {
                last_position_ = tick_callback_();	// update new mod tick
                mixer_samples_left_ = mixer_samples_per_tick_;
            }

            const int SamplesToMix = std::min(mixer_samples_left_, FSOUND_BlockSize - MixedSoFar);

            mix(MixPtr, SamplesToMix);

            MixedSoFar += SamplesToMix;
            MixPtr += SamplesToMix * 2;
            mixer_samples_left_ -= SamplesToMix;

        }

        samples_mixed_ += MixedSoFar; // This is (and was before) approximated down by as much as 1ms per block

        FMUSIC_TimeInfo[FSOUND_Software_FillBlock].samples = samples_mixed_;
        FMUSIC_TimeInfo[FSOUND_Software_FillBlock].position = last_position_;
    }


    // ====================================================================================
    // CLIP AND COPY BLOCK TO OUTPUT BUFFER
    // ====================================================================================
    MixerClipCopy_Float32(FSOUND_MixBlock.data.get() + mixpos * 2, FSOUND_MixBuffer.get(), FSOUND_BlockSize);

    ++FSOUND_Software_FillBlock;

    if (FSOUND_Software_FillBlock >= totalblocks)
    {
        FSOUND_Software_FillBlock = 0;
    }
}
