
#include <minixm/playback.h>

IPlaybackDriver::IPlaybackDriver(unsigned int mix_rate, unsigned int buffer_size_ms, unsigned int latency) :
    mix_rate_{ mix_rate },
    block_size_{ (((mix_rate_ * latency / 1000) + 3) & 0xFFFFFFFC) },
    total_blocks_{ (buffer_size_ms / latency) * 2 },
    buffer_size_{ block_size_ * total_blocks_ }
{
}
