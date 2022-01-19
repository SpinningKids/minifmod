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

/*
[API]
[
	[DESCRIPTION]
	Size optimized version of the commented out clipper below

	[PARAMETERS]

	[RETURN_VALUE]

	[REMARKS]
	This uses an fadd trick and does 2 float to ints in about 8 cycles.. which is what just 1
	fistp would take normally.. note this is meant for p5 machines, as ppro has a 1cycle
	fistp which is faster.

	[SEE_ALSO]
]
*/
void FSOUND_MixerClipCopy_Float32(short* dest, const float* src, int len)
{
	if (len <=0 || !dest || !src)
		return;

	for (int count = 0; count<len<<1; count++)
	{
		*dest++ = (short)std::clamp(*src++, -32768.f, 32767.f);
	}

}
