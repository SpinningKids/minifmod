/******************************************************************************/
/* MIXER_CLIPCOPY.C                                                           */
/* ----------------                                                           */
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/

#include <algorithm>
#include "mixer_clipcopy.h"

#include <cassert>

/*
[API]
[
	[DESCRIPTION]
	Size optimized version of the commented out clipper below

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]

	[SEE_ALSO]
]
*/
void FSOUND_MixerClipCopy_Float32(int16_t* dest, const float* src, size_t len) noexcept
{
	assert(src);
	assert(dest);
    for (size_t count = 0; count < len << 1; count++)
    {
        *dest++ = (int16_t)std::clamp((int) *src++, (int)std::numeric_limits<int16_t>::min(), (int)std::numeric_limits<int16_t>::max());
    }
}
