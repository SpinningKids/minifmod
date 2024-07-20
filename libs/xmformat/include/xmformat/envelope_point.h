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

#pragma pack(push)
#pragma pack(1)

static_assert(std::endian::native == std::endian::little); // this will only work in little endian system

struct XMEnvelopePoint
{
    uint16_t position;
    uint16_t value;
};

static_assert(sizeof(XMEnvelopePoint) == 4);

#pragma pack(pop)
