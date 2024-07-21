/******************************************************************************/
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/
/* This library (minixm) is maintained by Pan/SpinningKids, 2022-2024         */
/******************************************************************************/

#pragma once

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
