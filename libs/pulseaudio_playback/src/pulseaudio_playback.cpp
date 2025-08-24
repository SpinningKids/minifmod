/******************************************************************************/
// pulseaudio_playback by Pan/SpinningKids, 2025
/******************************************************************************/

#include <pulseaudio_playback/pulseaudio_playback.h>
#include <pulse/pulseaudio.h>

PulseAudioPlayback::PulseAudioPlayback(unsigned int mix_rate, unsigned int buffer_size_ms, unsigned int latency)
    : IPlaybackDriver(mix_rate, buffer_size_ms, latency),
      loop_(nullptr),
      context_(nullptr),
      stream_(nullptr),
      fill_func_(nullptr),
      fill_arg_(nullptr),
      current_block_(0),
      running_(true)
{
    loop_ = pa_mainloop_new();
    api_ = pa_mainloop_get_api(loop_);
    context_ = pa_context_new(api_, "minifmod");

    if (!context_) {
        if (loop_) {
            pa_mainloop_free(loop_);
            loop_ = nullptr;
        }
        return;
    }

    pa_context_connect(context_, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

    // Wait for context to be ready
    while (true) {
        pa_context_state_t state = pa_context_get_state(context_);
        if (state == PA_CONTEXT_READY)
            break;
        if (!PA_CONTEXT_IS_GOOD(state)) {
            pa_context_disconnect(context_);
            pa_context_unref(context_);
            pa_mainloop_free(loop_);
            context_ = nullptr;
            loop_ = nullptr;
            return;
        }
        pa_mainloop_iterate(loop_, 1, nullptr);
    }

    pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16LE;
    ss.rate = mix_rate;
    ss.channels = 2;

    stream_ = pa_stream_new(context_, "Playback", &ss, nullptr);
    if (!stream_) {
        pa_context_disconnect(context_);
        pa_context_unref(context_);
        pa_mainloop_free(loop_);
        context_ = nullptr;
        loop_ = nullptr;
        return;
    }

    buffer_.reset(new int16_t[block_size() * 2]);

    pa_stream_set_write_callback(stream_, &PulseAudioPlayback::pa_write_cb, this);

    pa_stream_flags_t stream_flags;
    stream_flags = pa_stream_flags_t(PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_NOT_MONOTONIC |
                 PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_ADJUST_LATENCY);
    pa_stream_connect_playback(stream_, nullptr, nullptr, stream_flags, nullptr, nullptr);

    // Wait for stream to be ready, with a timeout
    int max_wait = 100; // 100 * 10ms = 1 second
    bool ready = false;
    while (max_wait-- > 0) {
        pa_stream_state_t state = pa_stream_get_state(stream_);
        if (state == PA_STREAM_READY) {
            ready = true;
            break;
        }
        if (!PA_STREAM_IS_GOOD(state)) {
            pa_stream_disconnect(stream_);
            pa_stream_unref(stream_);
            pa_context_disconnect(context_);
            pa_context_unref(context_);
            pa_mainloop_free(loop_);
            stream_ = nullptr;
            context_ = nullptr;
            loop_ = nullptr;
            return;
        }
        pa_mainloop_iterate(loop_, 1, nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!ready) {
        pa_stream_disconnect(stream_);
        pa_stream_unref(stream_);
        pa_context_disconnect(context_);
        pa_context_unref(context_);
        pa_mainloop_free(loop_);
        stream_ = nullptr;
        context_ = nullptr;
        loop_ = nullptr;
        return;
    }
}

void PulseAudioPlayback::start(FillFunction* fill, void* arg)
{
    //unkork

    fill_func_ = fill;
    fill_arg_ = arg;
    current_block_ = 0;

    if (!stream_) {
        return;
    }

    running_ = true;
    if (!mainloop_thread_.joinable()) {
        mainloop_thread_ = std::thread([this] {
            int ret;
            while (running_) {
                pa_mainloop_iterate(loop_, 1, &ret);
            }
        });
    }
}

void PulseAudioPlayback::stop()
{
    running_ = false;
    if (mainloop_thread_.joinable()) mainloop_thread_.join();

    if (stream_) {
        pa_stream_disconnect(stream_);
        pa_stream_unref(stream_);
        stream_ = nullptr;
    }
    if (context_) {
        pa_context_disconnect(context_);
        pa_context_unref(context_);
        context_ = nullptr;
    }
    if (loop_) {
        pa_mainloop_free(loop_);
        loop_ = nullptr;
    }
}

PulseAudioPlayback::~PulseAudioPlayback()
{
    stop();
}

void PulseAudioPlayback::pa_write_cb(pa_stream* s, size_t nbytes, void* userdata)
{
    auto* self = static_cast<PulseAudioPlayback*>(userdata);
    auto block_bytes = self->block_size() * 2 * sizeof(buffer_[0]); // stereo 16-bit

    if (!self || !self->buffer_) {
        return;
    }

    while (nbytes >= block_bytes) {
        nbytes -= block_bytes;

        // Fill the block
        if (self->fill_func_) {
            self->fill_func_(self->fill_arg_, self->current_block_, self->buffer_.get());
        }

        pa_stream_write(s,
                        self->buffer_.get(),
                        block_bytes,
                        nullptr,
                        0,
                        PA_SEEK_RELATIVE);
        self->current_block_ = (self->current_block_ + 1) % self->blocks();
    }
}

size_t PulseAudioPlayback::current_block_played() const
{
    pa_usec_t usec = 0;
    if (pa_stream_get_time(stream_, &usec) == 0) {
        return static_cast<size_t>((usec * mix_rate() / block_size() / 1000000ull) % blocks());
    }
    return 0;
}
