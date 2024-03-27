
#include <thread>
#include <minixm/playback.h>

#define NOMINMAX
#include <Windows.h>

class WindowsPlayback final : public IPlaybackDriver
{
    std::unique_ptr<short[]> buffer_;

    HWAVEOUT					wave_out_handle_ = nullptr;

    std::thread					software_thread_;
    bool						software_thread_exit_ = true;		// mixing thread termination flag

public:

    WindowsPlayback() = delete;
    WindowsPlayback(const WindowsPlayback&) = delete;
    WindowsPlayback(WindowsPlayback&&) = delete;
    WindowsPlayback& operator = (const WindowsPlayback&) = delete;
    WindowsPlayback& operator = (WindowsPlayback&&) = delete;

    explicit WindowsPlayback(unsigned int mix_rate, unsigned int buffer_size_ms, unsigned int latency) :
        IPlaybackDriver(mix_rate, buffer_size_ms, latency),
        buffer_{ std::make_unique_for_overwrite<short[]>(buffer_size() * 2) }
    {}

    void start(std::function<void(size_t block, short data[])>&& fill) override
    {
        if (software_thread_exit_) {
            software_thread_exit_ = false;
            software_thread_ = std::thread{
                [this, fill = std::move(fill)]()
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

                    WAVEHDR		wave_header{
                        .lpData = reinterpret_cast<LPSTR>(buffer_.get()),
                        .dwBufferLength = buffer_size() * sizeof(short) * 2,
                        .dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP,
                        .dwLoops = static_cast<DWORD>(-1)
                    };
                    waveOutPrepareHeader(wave_out_handle_, &wave_header, sizeof(WAVEHDR));

                    // pre-fill
                    for (DWORD i = 0; i < blocks(); ++i)
                    {
                        fill(i, buffer_.get() + i * block_size() * 2);
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
                            fill(software_fill_block, buffer_.get() + software_fill_block * block_size() * 2);
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

    void stop() override
    {
        // wait until callback has settled down and exited
        if (software_thread_.joinable())
        {
            // Kill the thread
            software_thread_exit_ = true;
            software_thread_.join();
        }
    }

    ~WindowsPlayback() override
    {
        stop();
    }

    [[nodiscard]] size_t current_block_played() const override
    {
        MMTIME	mmt{
            .wType = TIME_SAMPLES
        };
        waveOutGetPosition(wave_out_handle_, &mmt, sizeof(MMTIME));
        return (static_cast<size_t>(mmt.u.cb) / block_size()) % blocks();
    }
};

std::unique_ptr<IPlaybackDriver> IPlaybackDriver::create(unsigned int mix_rate, unsigned int buffer_size_ms, unsigned int latency)
{
    return std::make_unique<WindowsPlayback>(mix_rate, buffer_size_ms, latency);
}

IPlaybackDriver::IPlaybackDriver(unsigned int mix_rate, unsigned int buffer_size_ms, unsigned int latency) :
    mix_rate_{ mix_rate },
    block_size_{ (((mix_rate_ * latency / 1000) + 3) & 0xFFFFFFFC) },
    total_blocks_{ (buffer_size_ms / latency) * 2 },
    buffer_size_{ block_size_ * total_blocks_ }
{
}