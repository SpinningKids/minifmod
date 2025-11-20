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

#include <minixm/mixer_channel.h>

#include <cmath>

void MixerChannel::mix(float* mixptr, uint32_t len, float filter_k)
{
    if (!sample_ptr)
    {
        return;
    }
    uint32_t sample_index = 0;
    const auto loop_start = static_cast<float>(sample_ptr->header.loop_start);
    const auto loop_length = static_cast<float>(sample_ptr->header.loop_length);
    auto loop_end = static_cast<float>(loop_start + loop_length);

    // Fixes potential bug: previously mix_positionwe play the sample until the end, and we wrapped back into the loop
    const auto loop_mode = (speed > 0 && mix_position > loop_end) ? XMLoopMode::Off : sample_ptr->header.loop_mode;
    if (loop_mode == XMLoopMode::Off) {
        loop_end = static_cast<float>(sample_ptr->header.length);
    }
    auto sample_target = (speed > 0) ? loop_end : loop_start;

    //==============================================================================================
    // LOOP THROUGH CHANNELS
    //==============================================================================================
    while (len > sample_index)
    {
        // Ensure that we don't try to mix a negative amount of samples
        const auto samples_to_mix_target = static_cast<uint32_t>(ceilf(std::max(
            0.f, (sample_target - mix_position) / speed))); // round up the division

        // =========================================================================================
        // the following code sets up a mix counter. it sees what will happen first, will the output buffer
        // end be reached first or will the end of the sample be reached first?
        // whatever is smallest will be the mix_count.
        const auto mix_count = std::min(len - sample_index, samples_to_mix_target);

        //= SET UP VOLUME MULTIPLIERS ==================================================

        for (uint32_t i = 0; i < mix_count; ++i)
        {
            const auto mixpos = static_cast<uint32_t>(mix_position);
            const float frac = mix_position - static_cast<float>(mixpos);
            const auto samp0 = static_cast<float>(sample_ptr->buff[mixpos]);
            const auto samp1 = static_cast<float>(sample_ptr->buff[mixpos + 1]);
            const auto newsamp = (samp1 - samp0) * frac + samp0;
            mixptr[0 + (sample_index + i) * 2] += filtered_left_volume * newsamp;
            mixptr[1 + (sample_index + i) * 2] += filtered_right_volume * newsamp;
            filtered_left_volume += (left_volume - filtered_left_volume) * filter_k;
            filtered_right_volume += (right_volume - filtered_right_volume) * filter_k;
            mix_position += speed;
        }

        sample_index += mix_count;

        //=============================================================================================
        // SWITCH ON LOOP MODE TYPE
        //=============================================================================================
        if (mix_count == samples_to_mix_target)
        {
            switch (loop_mode)
            {
            case XMLoopMode::Normal:
                do
                {
                    mix_position -= loop_length;
                } while (mix_position >= loop_end);
                break;
            case XMLoopMode::Bidi:
                do
                {
                    mix_position = 2 * sample_target - mix_position - 1;
                    speed = -speed;
                    sample_target = (speed > 0) ? loop_end : loop_start;
                } while ((sample_target - mix_position) * speed < 0.f);
                break;
            case XMLoopMode::Off:
            default:
                mix_position = 0;
                sample_ptr = nullptr;
                return;
            }
        }
    }
}
