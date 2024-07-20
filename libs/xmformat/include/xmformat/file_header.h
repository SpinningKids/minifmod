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

constexpr uint16_t FMUSIC_XMFLAGS_LINEARFREQUENCY = 1;

struct XMHeader
{
    char header[17];
    char module_name[20];
    char id; // 0x01a
    char tracker_name[20];
    uint16_t tracker_version;
    uint32_t header_size;
    uint16_t song_length; // < 256
    uint16_t restart_position;
    uint16_t channels_count;
    uint16_t patterns_count; // < 256
    uint16_t instruments_count; // < 128
    uint16_t flags; // 	0 - Linear frequency table / Amiga freq.table
    uint16_t default_tempo;
    uint16_t default_bpm;
    uint8_t pattern_order[256];
};

static_assert(sizeof(XMHeader) == 60 + 20 + 256); // xm header is 336 bytes long

#pragma pack(pop)
