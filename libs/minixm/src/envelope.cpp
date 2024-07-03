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

#include <minixm/envelope.h>

void EnvelopeState::process(const EnvelopePoints& envelope, XMEnvelopeFlags flags, unsigned char loop_start_index, unsigned char loop_end_index, unsigned char sustain_index, bool keyoff)
{
    if (envelope.count > 0)
    {

        if ((flags & XMEnvelopeFlagsLoop) && position_ == envelope.envelope[loop_end_index].position)	// if we are at the correct tick for the position
        {
            // loop
            position_ = envelope.envelope[loop_start_index].position;
        }
        // search new envelope position
        int current_index = 0;
        while (current_index + 1 < envelope.count && position_ > envelope.envelope[current_index + 1].position) current_index++;

        // interpolate new envelope position

        value_ = envelope.envelope[current_index].value + envelope.envelope[current_index].delta * static_cast<float>(position_ - envelope.envelope[current_index].position);

        // Envelope
        // if it is at the last position, abort the envelope and continue last value
        // same if we're at sustain point
        if (keyoff || current_index != sustain_index || !(flags & XMEnvelopeFlagsSustain))
        {
            ++position_;
        }
    }
}
