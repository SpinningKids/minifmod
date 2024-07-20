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

struct XMNote
{
    static constexpr uint8_t KEY_OFF = 97;
    uint8_t value{}; // note to play at     (0-133) 132=none,133=keyoff <- This comment is WRONG.

    [[nodiscard]] bool isKeyOff() const noexcept { return value == KEY_OFF; }
    [[nodiscard]] bool isValid() const noexcept { return value && value != KEY_OFF; }

    [[nodiscard]] XMNote operator +(int8_t rel) const noexcept
    {
        return {static_cast<uint8_t>(value + rel)};
    }

    [[nodiscard]] XMNote operator -(int8_t rel) const noexcept
    {
        return {static_cast<uint8_t>(value - rel)};
    }

    void turnOff() noexcept { value = KEY_OFF; }
};

static_assert(sizeof(XMNote) == 1);

#pragma pack(pop)
