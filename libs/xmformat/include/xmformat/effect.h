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

enum class XMEffect : uint8_t
{
    ARPEGGIO,
    PORTA_UP,
    PORTA_DOWN,
    PORTATO,
    VIBRATO,
    PORTATO_VOLUME_SLIDE,
    VIBRATO_VOLUME_SLIDE,
    TREMOLO,
    SET_PAN_POSITION,
    SET_SAMPLE_OFFSET,
    VOLUME_SLIDE,
    PATTERN_JUMP,
    SET_VOLUME,
    PATTERN_BREAK,
    SPECIAL,
    SET_SPEED,
    SET_GLOBAL_VOLUME,
    GLOBAL_VOLUME_SLIDE,
    I,
    J,
    KEY_OFF,
    SET_ENVELOPE_POSITION,
    M,
    N,
    O,
    PAN_SLIDE,
    Q,
    MULTI_RETRIGGER,
    S,
    TREMOR,
    U,
    V,
    W,
    EXTRA_FINE_PORTA,
    Y,
    Z,
};

static_assert(sizeof(XMEffect) == 1);

enum class XMSpecialEffect : uint8_t
{
    SET_FILTER,
    FINE_PORTA_UP,
    FINE_PORTA_DOWN,
    SET_GLISSANDO,
    SET_VIBRATO_WAVE,
    SET_FINE_TUNE,
    PATTERN_LOOP,
    SET_TREMOLO_WAVE,
    SET_PAN_POSITION_16,
    RETRIGGER,
    FINE_VOLUME_SLIDE_UP,
    FINE_VOLUME_SLIDE_DOWN,
    NOTE_CUT,
    NOTE_DELAY,
    PATTERN_DELAY,
    FUNK_REPEAT,
};

static_assert(sizeof(XMSpecialEffect) == 1);

enum class XMRetriggerVolumeOperation : uint8_t
{
    NONE,
    DECREASE_1,
    DECREASE_2,
    DECREASE_4,
    DECREASE_8,
    DECREASE_16,
    SUBTRACT_33_PERCENT,
    HALVE,
    INCREASE_1,
    INCREASE_2,
    INCREASE_4,
    INCREASE_8,
    INCREASE_16,
    ADD_50_PERCENT,
    DOUBLE,
};

#pragma pack(pop)
