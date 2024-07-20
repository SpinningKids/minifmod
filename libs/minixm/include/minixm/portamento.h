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

#include <algorithm>

class Portamento
{
    int target_;
    int speed_;

public:
    void setTarget(int target) noexcept { target_ = target; }
    void setSpeed(int speed) noexcept { if (speed) speed_ = speed; }

    int operator ()(int period) const noexcept
    {
        return std::clamp(target_, period - speed_, period + speed_);
    }
};
