
#include <thread>
#include <minixm/playback.h>

#define NOMINMAX
#include <Windows.h>

class WindowsPlayback final : public IPlaybackDriver
{
    std::unique_ptr<short[]> buffer_;

    HWAVEOUT wave_out_handle_ = nullptr;

    std::thread software_thread_;
    bool software_thread_exit_ = true; // mixing thread termination flag

public:
    WindowsPlayback() = delete;
    WindowsPlayback(const WindowsPlayback&) = delete;
    WindowsPlayback(WindowsPlayback&&) = delete;
    WindowsPlayback& operator =(const WindowsPlayback&) = delete;
    WindowsPlayback& operator =(WindowsPlayback&&) = delete;

    WindowsPlayback(unsigned int mix_rate, unsigned int buffer_size_ms = 1000, unsigned int latency = 20);

    void start(FillFunction* fill, void* arg) override;

    void stop() override;

    ~WindowsPlayback() override;

    [[nodiscard]] size_t current_block_played() const override;
};
