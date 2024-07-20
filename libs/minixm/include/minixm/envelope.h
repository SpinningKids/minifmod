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

#include <xmformat/envelope_flags.h>

struct EnvelopePoint
{
    int position;
    float value;
    float delta;
};

struct EnvelopePoints
{
    int count;
    EnvelopePoint envelope[12];
};

class EnvelopeState
{
    int position_;
    float value_;

public:
    void reset(float value) noexcept
    {
        position_ = 0;
        value_ = value;
    }

    void process(const EnvelopePoints& envelope, XMEnvelopeFlags flags, unsigned char loop_start_index,
                 unsigned char loop_end_index, unsigned char sustain_index, bool keyoff);
    float operator()() const noexcept { return value_; }
    void setPosition(int position) noexcept { position_ = position; }
};
