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

#pragma once

#include <numbers>

enum class WaveControl
{
    Sine,
    SawTooth,
    Square,
    Random,
};

class LFO final
{
    WaveControl wave_control_;
    bool continue_;
    int position_;
    int speed_;
    int depth_;

public:
    void setSpeed(int speed) noexcept { if (speed) speed_ = speed; }
    void setDepth(int depth) noexcept { if (depth) depth_ = depth; }

    void setFlags(int flags) noexcept
    {
        wave_control_ = static_cast<WaveControl>(flags & 3);
        continue_ = (flags & 4) != 0;
    }

    void reset() noexcept
    {
        if (!continue_)
        {
            position_ = 0;
        }
    }

    void update() noexcept
    {
        position_ += speed_;
        while (position_ > 31)
        {
            position_ -= 64;
        }
    }

    [[nodiscard]] int operator ()() const noexcept
    {
        switch (wave_control_)
        {
        case WaveControl::Sine:
            return static_cast<int>(sinf(static_cast<float>(position_) * (std::numbers::pi_v<float> / 32.0f)) *
                static_cast<float>(depth_));
        case WaveControl::SawTooth:
            return -(position_ * 2 + 1) * depth_ / 63;
        default: // handles WaveControl::Square and WaveControl::Random:
            return (position_ >= 0) ? -depth_ : depth_; // square
        }
    }
};
