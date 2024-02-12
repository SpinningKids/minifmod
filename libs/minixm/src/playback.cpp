
#include <minixm/playback.h>

float Playback::timeFromSamples() const noexcept
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

void Playback::double_buffer_thread() noexcept
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
            software_real_block_ = (software_real_block_ + 1) % total_blocks_;
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

void Playback::fill() noexcept
{
    // calculate which block to fill
    const unsigned int mixpos = software_fill_block_ * block_size_;
    auto target = mix_block_.data.get() + static_cast<size_t>(mixpos) * 2;

    // run the filler
    time_info_[software_fill_block_] = do_fill_(target);

    // increment block

    software_fill_block_ = (software_fill_block_ + 1) % total_blocks_;
}

