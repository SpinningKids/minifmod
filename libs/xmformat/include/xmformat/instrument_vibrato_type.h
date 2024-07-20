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

#include <cstdint>

#pragma pack(push)
#pragma pack(1)

enum class XMInstrumentVibratoType : uint8_t
{
    Sine = 0,
    Square = 1,
    InverseSawTooth = 2,
    SawTooth = 3
};

static_assert(sizeof(XMInstrumentVibratoType) == 1);

#pragma pack(pop)
