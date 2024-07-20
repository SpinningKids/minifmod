/******************************************************************************/
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/
/* This library (xmformat) is maintained by Pan/SpinningKids, 2022-2024       */
/******************************************************************************/

#pragma once

#include <bit>
#include <cstdint>

#include "envelope_flags.h"
#include "envelope_point.h"
#include "instrument_vibrato_type.h"

#pragma pack(push)
#pragma pack(1)

static_assert(std::endian::native == std::endian::little); // this will only work in little endian system

struct XMInstrumentSampleHeader
{
    uint32_t header_size; // sample header size
    uint8_t note_sample_number[96]; // sample numbers for notes
    XMEnvelopePoint volume_envelope[12]; // volume envelope points
    XMEnvelopePoint pan_envelope[12]; // panning envelope points
    uint8_t volume_envelope_count; // number of volume envelope points
    uint8_t pan_envelope_count; // number of panning env. points
    uint8_t volume_sustain_index; // volume sustain point
    uint8_t volume_loop_start_index; // volume loop start point
    uint8_t volume_loop_end_index; // volume loop end point
    uint8_t pan_sustain_index; // panning sustain point
    uint8_t pan_loop_start_index; // panning loop start point
    uint8_t pan_loop_end_index; // panning loop end point
    XMEnvelopeFlags volume_envelope_flags; // volume envelope flags
    XMEnvelopeFlags pan_envelope_flags; // panning envelope flags
    XMInstrumentVibratoType vibrato_type; // vibrato type
    uint8_t vibrato_sweep; // vibrato sweep
    uint8_t vibrato_depth; // vibrato depth
    uint8_t vibrato_rate; // vibrato rate
    uint16_t volume_fadeout; // volume fadeout
    uint16_t reserved;
};

static_assert(sizeof(XMInstrumentSampleHeader) == 214);

#pragma pack(pop)
