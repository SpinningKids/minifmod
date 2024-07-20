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

enum XMEnvelopeFlags : uint8_t
{
    XMEnvelopeFlagsOff = 0,
    XMEnvelopeFlagsOn = 1,
    XMEnvelopeFlagsSustain = 2,
    XMEnvelopeFlagsLoop = 4
};

static_assert(sizeof(XMEnvelopeFlags) == 1);

#pragma pack(pop)
