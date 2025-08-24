/******************************************************************************/
//           pulseaudio_playback by Pan/SpinningKids, 2025
/******************************************************************************/

#pragma once

#include <cstdint>
#include <memory>
#include <thread>
#include <atomic>
#include <minixm/playback.h>
#include <pulse/pulseaudio.h>

class PulseAudioPlayback : public IPlaybackDriver {
public:
    PulseAudioPlayback(unsigned int mix_rate, unsigned int buffer_size_ms = 1000, unsigned int latency = 20);
    ~PulseAudioPlayback();

    void start(FillFunction* fill, void* arg) override;
    void stop() override;
    size_t current_block_played() const override;

private:
    static void pa_write_cb(pa_stream* s, size_t nbytes, void* userdata);

    pa_mainloop* loop_;
    pa_mainloop_api* api_;
    pa_context* context_;
    pa_stream* stream_;

    FillFunction* fill_func_;
    void* fill_arg_;
    std::unique_ptr<int16_t[]> buffer_;
    size_t current_block_;

    std::thread mainloop_thread_;
    std::atomic<bool> running_;
};
