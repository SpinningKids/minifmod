/******************************************************************************/
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/
/* This library (winmm_playvack) is maintained by Pan/SpinningKids, 2022-2024 */
/******************************************************************************/

#include <winmm_playback/winmm_playback.h>

WindowsPlayback::WindowsPlayback(unsigned int mix_rate, unsigned int buffer_size_ms, unsigned int latency) :
    IPlaybackDriver(mix_rate, buffer_size_ms, latency),
    buffer_{ std::make_unique_for_overwrite<short[]>(buffer_size() * 2) }
{
}

void WindowsPlayback::start(FillFunction* fill, void* arg)
{
    if (software_thread_exit_)
    {
        software_thread_exit_ = false;
        software_thread_ = std::thread{
            [this, fill, arg]()
            {
                software_thread_exit_ = false;

                // ========================================================================================================
                // INITIALIZE WAVEOUT
                // ========================================================================================================
                const WAVEFORMATEX wave_format{
                    .wFormatTag = WAVE_FORMAT_PCM,
                    .nChannels = 2,
                    .nSamplesPerSec = mix_rate(),
                    .nAvgBytesPerSec = mix_rate() * 4,
                    .nBlockAlign = 4,
                    .wBitsPerSample = 16,
                    .cbSize = 0
                };

                if (waveOutOpen(&wave_out_handle_, WAVE_MAPPER, &wave_format, 0, 0, 0))
                {
                    software_thread_exit_ = true;
                    return;
                }
                // ========================================================================================================
                // PREFILL THE MIXER BUFFER
                // ========================================================================================================

                WAVEHDR wave_header{
                    .lpData = reinterpret_cast<LPSTR>(buffer_.get()),
                    .dwBufferLength = buffer_size() * sizeof(short) * 2,
                    .dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP,
                    .dwLoops = static_cast<DWORD>(-1)
                };
                waveOutPrepareHeader(wave_out_handle_, &wave_header, sizeof(WAVEHDR));

                // pre-fill
                for (DWORD i = 0; i < blocks(); ++i)
                {
                    fill(arg, i, buffer_.get() + i * block_size() * 2);
                }

                // ========================================================================================================
                // START THE OUTPUT
                // ========================================================================================================

                waveOutWrite(wave_out_handle_, &wave_header, sizeof(WAVEHDR));

                DWORD software_fill_block = 0;

                while (!software_thread_exit_)
                {
                    while (software_fill_block != current_block_played())
                    {
                        fill(arg, software_fill_block, buffer_.get() + software_fill_block * block_size() * 2);
                        software_fill_block = (software_fill_block + 1) % blocks();
                    }

                    Sleep(5);
                }

                waveOutUnprepareHeader(wave_out_handle_, &wave_header, sizeof(WAVEHDR));

                // ========================================================================================================
                // SHUT DOWN OUTPUT DRIVER
                // ========================================================================================================
                waveOutReset(wave_out_handle_);
                waveOutClose(wave_out_handle_);
                wave_out_handle_ = nullptr;
            }
        };
    }
}

void WindowsPlayback::stop()
{
    // wait until callback has settled down and exited
    if (software_thread_.joinable())
    {
        // Kill the thread
        software_thread_exit_ = true;
        software_thread_.join();
    }
}

WindowsPlayback::~WindowsPlayback()
{
    stop();
}

[[nodiscard]] size_t WindowsPlayback::current_block_played() const
{
    MMTIME mmt{
        .wType = TIME_SAMPLES
    };
    waveOutGetPosition(wave_out_handle_, &mmt, sizeof(MMTIME));
    return (static_cast<size_t>(mmt.u.cb) / block_size()) % blocks();
}
