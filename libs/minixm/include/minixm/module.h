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
#include <cstdint>

#include "channel.h"
#include "instrument.h"
#include "pattern.h"
#include "system_file.h"

#include <xmformat/file_header.h>

struct Module final
{
    XMHeader header_;
    Pattern pattern_[256]; // patterns array for this song
    Instrument instrument_[128]; // instrument array for this song (not used in MOD/S3M)

    using SampleLoadFunction = void(int16_t*, size_t, int, int);

    Module(const minifmod::FileAccess& fileAccess, void* fp,
           SampleLoadFunction* sample_load_callback);

    [[nodiscard]] const Instrument& getInstrument(int instrument) const
    {
        assert(instrument >= 0 && instrument < header_.instruments_count);
        return instrument_[instrument];
    }

    [[nodiscard]] Instrument& getInstrument(int instrument)
    {
        assert(instrument >= 0 && instrument < header_.instruments_count);
        return instrument_[instrument];
    }
};
