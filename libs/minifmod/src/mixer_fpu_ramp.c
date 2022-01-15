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

#include <minifmod/minifmod.h>
#include "Sound.h"
#include "Mixer.h"
#include "mixer_clipcopy.h"
#include "mixer_fpu_ramp.h"

#include <stdint.h>

#include "system_memory.h"

#pragma warning(disable:4731)

// =========================================================================================
// GLOBAL VARIABLES
// =========================================================================================

//= made global to free ebp.============================================================================
static unsigned int mix_endflag		= 0;
static float		mix_leftvol		= 0;
static float		mix_rightvol	= 0;

static unsigned int mix_count_old	= 0;
unsigned int		mix_volumerampsteps	= 0;
float				mix_1overvolumerampsteps = 0;

static const float	mix_255			= 255.0f;
static const float	mix_256m255		= 256.0f*255.0f;
static const float	mix_1over255	= 1.0f / 255.0f;
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

void FSOUND_Mixer_FPU_Ramp(void *mixptr, int len, char returnaddress)
{
	FSOUND_CHANNEL *cptr;
	int count;
	// IMPORTANT no local variables on stack.. we are trashing EBP.  static puts values on heap.

	if (len <=0)
		return;

	float* target = (float*)mixptr;
    float* target_end = target + (len << 1);


	//==============================================================================================
	// LOOP THROUGH CHANNELS
	//==============================================================================================
	for (count=0; count<64; count++)
	{
		cptr = &FSOUND_Channel[count];
        int sample_index = 0;
		if (!cptr->sptr)
            continue;
        unsigned int mix_count = len;

        //==============================================================================================
        // LOOP THROUGH CHANNELS
        // through setup code:- usually ebx = sample pointer, ecx = channel pointer
        //==============================================================================================

        //= SUCCESS - SETUP CODE FOR THIS CHANNEL ======================================================

        // =========================================================================================
        // the following code sets up a mix counter. it sees what will happen first, will the output buffer
        // end be reached first? or will the end of the sample be reached first? whatever is smallest will
        // be the mixcount.

        // first base mixcount on size of OUTPUT BUFFER (in samples not bytes)
        do
        {
            mix_endflag = FSOUND_OUTPUTBUFF_END;
            uint64_t samples_to_mix = (cptr->speeddir == FSOUND_MIXDIR_FORWARDS) ?
                                          (((uint64_t)(cptr->sptr->loopstart + cptr->sptr->looplen)) << 32) - cptr->mixpos64 :
                                          cptr->mixpos64 - (((uint64_t)cptr->sptr->loopstart) << 32);
            if (samples_to_mix < 0x100000000000000)
            {
                unsigned int samples = (samples_to_mix / cptr->speed64) + (((samples_to_mix % cptr->speed64) == 0)?0:1); // TODO: +1 if the remainder is 0
                if (samples <= mix_count)
                {
                    mix_count = samples;
                    mix_endflag = FSOUND_SAMPLEBUFF_END;
                }
            }
            //= VOLUME RAMP SETUP =========================================================
            // Reasons to ramp
            // 1. volume change
            // 2. sample starts (just treat as volume change - 0 to volume)
            // 3. sample ends (ramp last n number of samples from volume to 0)

            // now if the volume has changed, make end condition equal a volume ramp
            mix_count_old = mix_count;
            if (
                (cptr->ramp_count == 0) ||
                (cptr->leftvolume != cptr->ramp_lefttarget) ||
                (cptr->rightvolume != cptr->ramp_righttarget))
            {
                // if it tries to continue an old ramp, but the target has changed,
                // set up a new ramp
                cptr->ramp_lefttarget = cptr->leftvolume;
                int temp1 = cptr->leftvolume - (cptr->ramp_leftvolume >> 8);
                cptr->ramp_leftspeed = temp1 * mix_1over255 * mix_1overvolumerampsteps;
                cptr->ramp_righttarget = cptr->rightvolume;
                int temp2 = cptr->rightvolume - (cptr->ramp_rightvolume >> 8);
                cptr->ramp_rightspeed = temp2 * mix_1over255 * mix_1overvolumerampsteps;
                cptr->ramp_count = mix_volumerampsteps;
            }
            if (cptr->ramp_count > 0) {
                if (mix_count > cptr->ramp_count) {
                    mix_count = cptr->ramp_count;
                }
            }
            mix_rightvol = cptr->rightvolume * mix_1over255;
            mix_leftvol = cptr->leftvolume * mix_1over255;

            int64_t speed = cptr->speed64;

            if (cptr->speeddir != FSOUND_MIXDIR_FORWARDS)
            {
                speed = -speed;
            }

            //= SET UP VOLUME MULTIPLIERS ==================================================

            // left ramp volume
            float ramplvol = cptr->ramp_leftvolume * mix_1over256over255;

            // right ramp volume
            float ramprvol = cptr->ramp_rightvolume * mix_1over256over255;

            for (unsigned int i = 0; i < mix_count; ++i)
            {
                float samp1 = ((short*)(cptr->sptr->buff))[cptr->mixpos];
                float newsamp = (((short*)(cptr->sptr->buff))[cptr->mixpos + 1] - samp1) * cptr->mixposlo * mix_1over4gig + samp1;
                target[0 + (sample_index + i) * 2] += ramplvol * newsamp;
                target[1 + (sample_index + i) * 2] += ramprvol * newsamp;
                ramplvol += cptr->ramp_leftspeed;
                ramprvol += cptr->ramp_rightspeed;
                cptr->mixpos64 += speed;
            }

            sample_index += mix_count;


            //=============================================================================================
            // DID A VOLUME RAMP JUST HAPPEN
            //=============================================================================================
            if (cptr->ramp_count != 0) {
                cptr->ramp_leftvolume = (unsigned int)(ramplvol * mix_256m255);
                cptr->ramp_rightvolume = (unsigned int)(ramprvol * mix_256m255);
                cptr->ramp_count -= mix_count;

                // if rampcount now = 0, a ramp has FINISHED, so finish the rest of the mix
                if (cptr->ramp_count == 0)
                {

                    // clear out the ramp speeds
                    cptr->ramp_leftspeed = 0;
                    cptr->ramp_rightspeed = 0;

                    // clamp the 2 volumes together in case the speed wasnt accurate enough!
                    cptr->ramp_leftvolume = cptr->leftvolume << 8;
                    cptr->ramp_rightvolume = cptr->rightvolume << 8;

                    // is it 0 because ramp ended only? or both ended together??
                    // if sample ended together with ramp.. problems .. loop isnt handled

                    if (mix_count_old != mix_count) {

                        // start again and continue rest of mix
                        mix_count = len - sample_index;
                        if (mix_count != 0)
                        {
                            continue;
                        }
                    }
                }
            }
            //DoOutputbuffEnd
            if (mix_endflag == FSOUND_OUTPUTBUFF_END)
            {
                break;
            }
            //=============================================================================================
            // SWITCH ON LOOP MODE TYPE
            //=============================================================================================
            if (cptr->sptr->loopmode & FSOUND_LOOP_NORMAL)
            {
                unsigned target = cptr->sptr->loopstart + cptr->sptr->looplen;
                do
                {
                    cptr->mixpos -= cptr->sptr->looplen;
                } while (cptr->mixpos >= target);
            } else if (cptr->sptr->loopmode & FSOUND_LOOP_BIDI)
            {
                do {
                    if (cptr->speeddir != FSOUND_MIXDIR_FORWARDS)
                    {
                        //BidiBackwards
                        cptr->mixpos = 2 * cptr->sptr->loopstart - 1 - cptr->mixpos;
                        cptr->mixposlo = ~cptr->mixposlo;
                        cptr->speeddir = FSOUND_MIXDIR_FORWARDS;
                        if (cptr->mixpos < cptr->sptr->loopstart + cptr->sptr->looplen)
                        {
                            break;
                        }

                    }
                    //BidiForward
                    cptr->mixpos = 2 * (cptr->sptr->loopstart + cptr->sptr->looplen - 1) - cptr->mixpos + ((cptr->mixposlo == 0)?1:0);
                    cptr->mixposlo = ~cptr->mixposlo;
                    cptr->speeddir = FSOUND_MIXDIR_BACKWARDS;

                } while (cptr->mixpos < cptr->sptr->loopstart);
            } else
            {
                cptr->mixpos = 0;
                cptr->mixposlo = 0;
                cptr->sptr = 0;
                break;
            }
            mix_count = len - sample_index;
        } while (mix_count != 0);
    }
}
