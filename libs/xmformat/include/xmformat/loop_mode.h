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

enum class XMLoopMode : uint8_t
{
    Off = 0,
    Normal = 1, // For forward looping samples.
    Bidi = 2, // For bidirectional looping samples.  (no effect if in hardware).
};

static_assert(sizeof(XMLoopMode) == 1);

#pragma pack(pop)
