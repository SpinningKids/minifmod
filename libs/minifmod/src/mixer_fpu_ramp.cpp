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

#include <cstdint>

#include "Sound.h"
#include "Mixer.h"

// =========================================================================================
// GLOBAL VARIABLES
// =========================================================================================

//= made global to free ebp.============================================================================
static const float	mix_256m255		= 256.0f*255.0f;
static const float	mix_1over256over255	= 1.0f / 256.0f / 255.0f;
static const float	mix_1over4gig   = 1.0f / 4294967296.0f;

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
            if (
                (channel.ramp_count == 0) ||
                (channel.leftvolume != channel.ramp_lefttarget) ||
                (channel.rightvolume != channel.ramp_righttarget))
            {
                // if it tries to continue an old ramp, but the target has changed,
                // set up a new ramp
                channel.ramp_lefttarget = channel.leftvolume;
                channel.ramp_leftspeed = (float(channel.leftvolume) - (channel.ramp_leftvolume >> 8)) / (255 * mix_volumerampsteps);
                channel.ramp_righttarget = channel.rightvolume;
                channel.ramp_rightspeed = (float(channel.rightvolume) - (channel.ramp_rightvolume >> 8)) / (255 * mix_volumerampsteps);
                channel.ramp_count = mix_volumerampsteps;
            }
            if (channel.ramp_count > 0) {
                if (mix_count > channel.ramp_count) {
                    mix_count = channel.ramp_count;
                }
            }

            uint64_t samples_to_mix = (channel.speeddir == FSOUND_MIXDIR_FORWARDS) ?
                                          (((uint64_t)(channel.sptr->header.loop_start + channel.sptr->header.loop_length)) << 32) - channel.mixpos64 :
                                          channel.mixpos64 - (((uint64_t)channel.sptr->header.loop_start) << 32);
            uint64_t samples = (samples_to_mix + channel.speed64 - 1) / channel.speed64; // round up the division
            if (samples <= (uint64_t)mix_count)
            {
                mix_count = (uint32_t)samples;
                mix_endflag_sample = true;
            }

            int64_t speed = channel.speed64;

            if (channel.speeddir != FSOUND_MIXDIR_FORWARDS)
            {
                speed = -speed;
            }

            //= SET UP VOLUME MULTIPLIERS ==================================================

            // left ramp volume
            float ramplvol = channel.ramp_leftvolume * mix_1over256over255;

            // right ramp volume
            float ramprvol = channel.ramp_rightvolume * mix_1over256over255;

            for (unsigned int i = 0; i < mix_count; ++i)
            {
                uint32_t mixpos = (channel.mixpos64 >> 32);
                uint32_t mixposlo = (channel.mixpos64 & 0xffffffff);
                float samp1 = channel.sptr->buff[mixpos];
                float newsamp = (channel.sptr->buff[mixpos + 1] - samp1) * mixposlo * mix_1over4gig + samp1;
                mixptr[0 + (sample_index + i) * 2] += ramplvol * newsamp;
                mixptr[1 + (sample_index + i) * 2] += ramprvol * newsamp;
                ramplvol += channel.ramp_leftspeed;
                ramprvol += channel.ramp_rightspeed;
                channel.mixpos64 += speed;
            }

            sample_index += mix_count;

            //=============================================================================================
            // DID A VOLUME RAMP JUST HAPPEN
            //=============================================================================================
            if (channel.ramp_count != 0) {
                channel.ramp_leftvolume = (unsigned int)(ramplvol * mix_256m255);
                channel.ramp_rightvolume = (unsigned int)(ramprvol * mix_256m255);
                channel.ramp_count -= mix_count;

                // if rampcount now = 0, a ramp has FINISHED, so finish the rest of the mix
                if (channel.ramp_count == 0)
                {

                    // clear out the ramp speeds
                    channel.ramp_leftspeed = 0;
                    channel.ramp_rightspeed = 0;

                    // clamp the 2 volumes together in case the speed wasnt accurate enough!
                    channel.ramp_leftvolume = channel.leftvolume << 8;
                    channel.ramp_rightvolume = channel.rightvolume << 8;

                }
            }
            //=============================================================================================
            // SWITCH ON LOOP MODE TYPE
            //=============================================================================================
            if (mix_endflag_sample)
            {
                if (channel.sptr->header.loop_mode == FSOUND_XM_LOOP_NORMAL)
                {
                    unsigned target = channel.sptr->header.loop_start + channel.sptr->header.loop_length;
                    do
                    {
                        channel.mixpos64 -= uint64_t(channel.sptr->header.loop_length) << 32;
                    } while ((channel.mixpos64 >> 32) >= target);
                } else if (channel.sptr->header.loop_mode == FSOUND_XM_LOOP_BIDI)
                {
                    do {
                        if (channel.speeddir != FSOUND_MIXDIR_FORWARDS)
                        {
                            //BidiBackwards
                            channel.mixpos64 = (uint64_t(2 * channel.sptr->header.loop_start) << 32) - channel.mixpos64 - 1;
                            channel.speeddir = FSOUND_MIXDIR_FORWARDS;
                            if ((channel.mixpos64 >> 32) < channel.sptr->header.loop_start + channel.sptr->header.loop_length)
                            {
                                break;
                            }

                        }
                        //BidiForward
                        channel.mixpos64 = (uint64_t(2 * (channel.sptr->header.loop_start + channel.sptr->header.loop_length)) << 32) - channel.mixpos64 - 1;
                        channel.speeddir = FSOUND_MIXDIR_BACKWARDS;

                    } while ((channel.mixpos64 >> 32) < channel.sptr->header.loop_start);
                } else
                {
                    channel.mixpos64 = 0;
                    channel.sptr = nullptr;
                }
            }
        }
    }
}
