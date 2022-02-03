/******************************************************************************/
/* MIXER_FPU_RAMP.C                                                           */
/* ----------------                                                           */
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support      */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/

#include "mixer_fpu_ramp.h"

#include <cassert>
#include <cmath>
#include <cstdint>

#include "Sound.h"
#include "Mixer.h"

// =========================================================================================
// GLOBAL VARIABLES
// =========================================================================================

/*
[API]
[
	[DESCRIPTION]

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/

void FSOUND_Mixer_FPU_Ramp(float *mixptr, int len) noexcept
{
	//==============================================================================================
	// LOOP THROUGH CHANNELS
	//==============================================================================================
	for (auto& channel : FSOUND_Channel)
    {
        int sample_index = 0;

        //==============================================================================================
        // LOOP THROUGH CHANNELS
        //==============================================================================================
        while (channel.sptr && len > sample_index)
        {

            float samples_to_mix;
            if (channel.speeddir == FSOUND_MIXDIR_FORWARDS)
            {
                samples_to_mix = channel.sptr->header.loop_start + channel.sptr->header.loop_length - channel.mixpos;
                if (samples_to_mix <= 0)
                {
                    samples_to_mix = channel.sptr->header.length - channel.mixpos;
                }
            }
            else
            {
                samples_to_mix = channel.mixpos - channel.sptr->header.loop_start;
            }
            const int samples_to_mix_target = (int)ceil(samples_to_mix / channel.speed); // round up the division

            // =========================================================================================
            // the following code sets up a mix counter. it sees what will happen first, will the output buffer
            // end be reached first or will the end of the sample be reached first?
            // whatever is smallest will be the mixcount.
            int mix_count = std::min(len - sample_index, samples_to_mix_target);

            float speed = channel.speed;

            if (channel.speeddir != FSOUND_MIXDIR_FORWARDS)
            {
                speed = -speed;
            }

            //= SET UP VOLUME MULTIPLIERS ==================================================

            for (int i = 0; i < mix_count; ++i)
            {
                const uint32_t mixpos = (uint32_t)channel.mixpos;
                const float frac = channel.mixpos - mixpos;
                const float samp1 = channel.sptr->buff[mixpos];
                const float newsamp = (channel.sptr->buff[mixpos + 1] - samp1) * frac + samp1;
                mixptr[0 + (sample_index + i) * 2] += channel.filtered_leftvolume * newsamp;
                mixptr[1 + (sample_index + i) * 2] += channel.filtered_rightvolume * newsamp;
                channel.filtered_leftvolume += (channel.leftvolume - channel.filtered_leftvolume) * mix_filter_k;
                channel.filtered_rightvolume += (channel.rightvolume - channel.filtered_rightvolume) * mix_filter_k;
                channel.mixpos += speed;
            }

            sample_index += mix_count;

            //=============================================================================================
            // SWITCH ON LOOP MODE TYPE
            //=============================================================================================
            if (mix_count == samples_to_mix_target)
            {
                if (channel.sptr->header.loop_mode == FSOUND_XM_LOOP_NORMAL)
                {
                    const uint32_t target = channel.sptr->header.loop_start + channel.sptr->header.loop_length;
                    do
                    {
                        channel.mixpos -= channel.sptr->header.loop_length;
                    } while (channel.mixpos >= target);
                } else if (channel.sptr->header.loop_mode == FSOUND_XM_LOOP_BIDI)
                {
                    do {
                        if (channel.speeddir != FSOUND_MIXDIR_FORWARDS)
                        {
                            //BidiBackwards
                            channel.mixpos = 2 * channel.sptr->header.loop_start - channel.mixpos - 1;
                            channel.speeddir = FSOUND_MIXDIR_FORWARDS;
                            if (channel.mixpos < channel.sptr->header.loop_start + channel.sptr->header.loop_length)
                            {
                                break;
                            }
                        }
                        //BidiForward
                        channel.mixpos = 2 * (channel.sptr->header.loop_start + channel.sptr->header.loop_length) - channel.mixpos - 1;
                        channel.speeddir = FSOUND_MIXDIR_BACKWARDS;

                    } while (channel.mixpos < channel.sptr->header.loop_start);
                }
                else
                {
                    channel.mixpos = 0;
                    channel.sptr = nullptr;
                }
            }
        }
    }
}
