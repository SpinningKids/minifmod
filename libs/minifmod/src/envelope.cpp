
#include "envelope.h"
#include "xmeffects.h"

#if defined(FMUSIC_XM_VOLUMEENVELOPE_ACTIVE) || defined(FMUSIC_XM_PANENVELOPE_ACTIVE)
void EnvelopeState::process(const EnvelopePoints& envelope, XMEnvelopeFlags flags, unsigned char loop_start_index, unsigned char loop_end_index, unsigned char sustain_index, bool keyoff) noexcept
{
    if (envelope.count > 0)
    {

        if ((flags & XMEnvelopeFlagsLoop) && position_ == envelope.envelope[loop_end_index].position)	// if we are at the correct tick for the position
        {
            // loop
            position_ = envelope.envelope[loop_start_index].position;
        }
        // search new envelope position
        uint8_t current_index = 0;
        while (current_index + 1 < envelope.count && position_ > envelope.envelope[current_index + 1].position) current_index++;

        // interpolate new envelope position

        value_ = envelope.envelope[current_index].value + envelope.envelope[current_index].delta * (position_ - envelope.envelope[current_index].position);

        // Envelope
        // if it is at the last position, abort the envelope and continue last value
        // same if we're at sustain point
        if (keyoff || current_index != sustain_index || !(flags & XMEnvelopeFlagsSustain))
        {
            ++position_;
        }
    }
}
#endif // (FMUSIC_XM_VOLUMEENVELOPE_ACTIVE) || defined(FMUSIC_XM_PANENVELOPE_ACTIVE)
