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

#include "loop_mode.h"

#pragma pack(push)
#pragma pack(1)

static_assert(std::endian::native == std::endian::little); // this will only work in little endian system

struct XMSampleHeader
{
	uint32_t	length;
	uint32_t	loop_start;
	uint32_t	loop_length;
	uint8_t		default_volume;
	int8_t		fine_tune;
	XMLoopMode	loop_mode : 2;
	uint8_t		skip_bits_1 : 2;
	bool		bits16 : 1;
	uint8_t		default_panning;
	int8_t		relative_note;
	uint8_t		reserved;
	char		sample_name[22];
};

static_assert(sizeof(XMSampleHeader) == 40);

#pragma pack(pop)
