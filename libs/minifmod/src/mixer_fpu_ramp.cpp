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

void FSOUND_Mixer_FPU_Ramp(float *mixptr, int len)
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

        //= SUCCESS - SETUP CODE FOR THIS CHANNEL ======================================================

        while (channel.sptr && len > sample_index)
        {
            // =========================================================================================
            // the following code sets up a mix counter. it sees what will happen first, will the output buffer
            // end be reached first? or a volume ramp? or will the end of the sample be reached first?
            // whatever is smallest will be the mixcount.
            unsigned int mix_count = len - sample_index;
            bool mix_endflag_sample = false;

            //= VOLUME RAMP SETUP =========================================================
            // Reasons to ramp
            // 1. volume change
            // 2. sample starts (just treat as volume change - 0 to volume)
            // 3. sample ends (ramp last n number of samples from volume to 0)

            // now if the volume has changed, make end condition equal a volume ramp
            if (channel.volume_changed || channel.ramp_count == 0)
            {
                // if it tries to continue an old ramp, but the target has changed,
                // set up a new ramp
                channel.ramp_leftspeed = (channel.leftvolume - channel.ramp_leftvolume) / mix_volumerampsteps;
                channel.ramp_rightspeed = (channel.rightvolume - channel.ramp_rightvolume) / mix_volumerampsteps;
                channel.ramp_count = mix_volumerampsteps;
            }
            assert(channel.ramp_count > 0);
            //if (channel.ramp_count > 0) {
                if (mix_count > channel.ramp_count) {
                    mix_count = channel.ramp_count;
                }
            //}

            float samples_to_mix;
            if (channel.speeddir == FSOUND_MIXDIR_FORWARDS)
            {
                samples_to_mix = channel.sptr->header.loop_start + channel.sptr->header.loop_length - channel.mixpos;
                if (samples_to_mix <= 0) {
                    samples_to_mix = channel.sptr->header.length - channel.mixpos;
                }
            }
            else
            {
                samples_to_mix = channel.mixpos - channel.sptr->header.loop_start;
            }
            const uint32_t samples = (uint32_t)ceil(samples_to_mix / channel.speed); // round up the division
            if (samples <= mix_count)
            {
                mix_count = samples;
                mix_endflag_sample = true;
            }

            float speed = channel.speed;

            if (channel.speeddir != FSOUND_MIXDIR_FORWARDS)
            {
                speed = -speed;
            }

            //= SET UP VOLUME MULTIPLIERS ==================================================

            for (unsigned int i = 0; i < mix_count; ++i)
            {
                const uint32_t mixpos = (uint32_t)channel.mixpos;
                const float frac = channel.mixpos - mixpos;
                const float samp1 = channel.sptr->buff[mixpos];
                const float newsamp = (channel.sptr->buff[mixpos + 1] - samp1) * frac + samp1;
                mixptr[0 + (sample_index + i) * 2] += channel.ramp_leftvolume * newsamp;
                mixptr[1 + (sample_index + i) * 2] += channel.ramp_rightvolume * newsamp;
                channel.ramp_leftvolume += channel.ramp_leftspeed;
                channel.ramp_rightvolume += channel.ramp_rightspeed;
                channel.mixpos += speed;
            }

            sample_index += mix_count;

            //=============================================================================================
            // DID A VOLUME RAMP JUST HAPPEN
            //=============================================================================================
            if (channel.ramp_count != 0) {
                channel.ramp_count -= mix_count;

                // if rampcount now = 0, a ramp has FINISHED, so finish the rest of the mix
                if (channel.ramp_count == 0)
                {

                    // clear out the ramp speeds
                    channel.ramp_leftspeed = 0;
                    channel.ramp_rightspeed = 0;

                    // clamp the 2 volumes together in case the speed wasnt accurate enough!
                    channel.ramp_leftvolume = channel.leftvolume;
                    channel.ramp_rightvolume = channel.rightvolume;

                }
            }
            //=============================================================================================
            // SWITCH ON LOOP MODE TYPE
            //=============================================================================================
            if (mix_endflag_sample)
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
                } else
                {
                    channel.mixpos = 0;
                    channel.sptr = nullptr;
                }
            }
        }
    }
}
