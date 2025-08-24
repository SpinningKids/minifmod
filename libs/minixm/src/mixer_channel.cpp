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
#include <limits>

void MixerChannel::mix(float* mixptr, uint32_t len, float filter_k)
{
    uint32_t sample_index = 0;

    //==============================================================================================
    // LOOP THROUGH CHANNELS
    //==============================================================================================
    while (sample_ptr && len > sample_index)
    {
        const auto loop_end = static_cast<float>(sample_ptr->header.loop_start + sample_ptr->header.
            loop_length);

        float samples_to_mix; // This can occasionally be < 0
        if (speed > 0)
        {
            samples_to_mix = loop_end - mix_position;
            if (samples_to_mix <= 0)
            {
                samples_to_mix = static_cast<float>(sample_ptr->header.length) - mix_position;
            }
        }
        else
        {
            samples_to_mix = mix_position - static_cast<float>(sample_ptr->header.loop_start);
        }

        // Ensure that we don't try to mix a negative amount of samples
        const auto samples_to_mix_target = static_cast<unsigned int>(std::max(
            0.f, ceilf(samples_to_mix / fabsf(speed)))); // round up the division

        // =========================================================================================
        // the following code sets up a mix counter. it sees what will happen first, will the output buffer
        // end be reached first or will the end of the sample be reached first?
        // whatever is smallest will be the mix_count.
        const unsigned int mix_count = std::min(len - sample_index, samples_to_mix_target);

        //= SET UP VOLUME MULTIPLIERS ==================================================

        for (unsigned int i = 0; i < mix_count; ++i)
        {
            const auto mixpos = static_cast<uint32_t>(mix_position);
            const float frac = mix_position - static_cast<float>(mixpos);
            const auto samp1 = sample_ptr->buff[mixpos];
            const float newsamp = static_cast<float>(sample_ptr->buff[mixpos + 1] - samp1) * frac +
                static_cast<float>(samp1);
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
            if (sample_ptr->header.loop_mode == XMLoopMode::Normal)
            {
                do
                {
                    mix_position -= static_cast<float>(sample_ptr->header.loop_length);
                } while (mix_position >= loop_end);
            }
            else if (sample_ptr->header.loop_mode == XMLoopMode::Bidi)
            {
                do
                {
                    if (speed < 0.f)
                    {
                        //BidiBackwards
                        mix_position = static_cast<float>(2 * sample_ptr->header.loop_start) -
                            mix_position - 1;
                        speed = -speed;
                        if (mix_position < loop_end)
                        {
                            break;
                        }
                    }
                    //BidiForward
                    mix_position = 2 * loop_end - mix_position - 1;
                    speed = -speed;
                } while (mix_position < static_cast<float>(sample_ptr->header.loop_start));
            }
            else
            {
                mix_position = 0;
                sample_ptr = nullptr;
            }
        }
    }
}

