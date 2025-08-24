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

#include <cassert>
#include <cmath>
#include <numbers>

#include "envelope.h"
#include "sample.h"
#include "xmeffects.h"

#include <xmformat/instrument_header.h>
#include <xmformat/instrument_sample_header.h>
#include <xmformat/note.h>

// Multi sample extended instrument
struct Instrument final
{
    XMInstrumentHeader header;
    XMInstrumentSampleHeader instrument_sample_header;
#ifdef FMUSIC_XM_VOLUMEENVELOPE_ACTIVE
    EnvelopePoints volume_envelope;
#endif
#ifdef FMUSIC_XM_PANENVELOPE_ACTIVE
    EnvelopePoints pan_envelope;
#endif
    Sample sample[16]; // 16 samples per instrument

    [[nodiscard]] const Sample& getSample(XMNote note) const
    {
        if (note.isValid())
        {
            assert(note.value < XMNote::KEY_OFF);
            const int note_sample = instrument_sample_header.note_sample_number[note.value - 1];
            assert(note_sample >= 0 && note_sample < 16);
            return sample[note_sample];
        }
        return sample[0];
    }

#ifdef FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE
    int instrument_vibrato_position; // instrument vibrato position
    int instrument_vibrato_sweep_position; // instrument vibrato sweep position

    [[nodiscard]] int getInstrumentVibratoDelta()
    {
        int delta = 0;

        switch (instrument_sample_header.vibrato_type)
        {
        case XMInstrumentVibratoType::Sine:
            {
                delta = static_cast<int>(sinf(
                    static_cast<float>(instrument_vibrato_position) * (std::numbers::pi_v<float> / 128.0f)) * 256.0f);
                break;
            }
        case XMInstrumentVibratoType::Square:
            {
                delta = 256 - (instrument_vibrato_position & 128) * 4;
                break;
            }
        case XMInstrumentVibratoType::InverseSawTooth:
            {
                delta = (instrument_vibrato_position & 128) * 4 - (instrument_vibrato_position + 1);
                break;
            }
        case XMInstrumentVibratoType::SawTooth:
            {
                delta = instrument_vibrato_position + 1 - (instrument_vibrato_position & 128) * 4;
                break;
            }
        }

        delta *= instrument_sample_header.vibrato_depth;
        if (instrument_sample_header.vibrato_sweep)
        {
            delta *= instrument_vibrato_sweep_position;
            delta /= instrument_sample_header.vibrato_sweep;
        }

        instrument_vibrato_sweep_position = std::min(instrument_vibrato_sweep_position + 1,
                                                     static_cast<int>(instrument_sample_header.vibrato_sweep));
        instrument_vibrato_position += instrument_sample_header.vibrato_rate;

        return delta / 128;
    }
#endif

    void reset()
    {
#ifdef FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE
        instrument_vibrato_sweep_position = 0;
        instrument_vibrato_position = 0;
#endif
    }
};
