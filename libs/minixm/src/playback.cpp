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

#include <minixm/playback.h>

IPlaybackDriver::IPlaybackDriver(unsigned int mix_rate, unsigned int buffer_size_ms, unsigned int latency) :
    mix_rate_{ mix_rate },
    block_size_{ (((mix_rate_ * latency / 1000) + 3) & 0xFFFFFFFC) },
    total_blocks_{ (buffer_size_ms / latency) * 2 },
    buffer_size_{ block_size_ * total_blocks_ }
{
}
