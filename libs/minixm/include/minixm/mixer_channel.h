
#include <cstdint>

#include "sample.h"

struct MixerChannel final
{
    unsigned int sample_offset; // sample offset (sample starts playing from here).

    const Sample* sample_ptr; // currently playing sample

    // software mixer stuff
    float left_volume; // mixing information. adjusted volume for left channel (panning involved)
    float right_volume; // mixing information. adjusted volume for right channel (panning involved)
    float mix_position; // mixing information. floating point fractional position in sample.
    float speed; // mixing information. playback rate - floating point.

    // software mixer volume ramping stuff
    float filtered_left_volume;
    float filtered_right_volume;

    void mix(float* mixptr, uint32_t len, float filter_k);
};
