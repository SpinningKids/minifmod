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

#include "module.h"
#include "mixer.h"
#include "position.h"
#include "xmeffects.h"

// Song type - contains info on song
struct PlayerState final
{
private:
    Channel channels_[32]{}; // channel array for this song

    std::unique_ptr<Module> module_;
    Mixer mixer_;
    int global_volume_; // global mod volume
    int tick_; // current mod tick
    int ticks_per_row_; // speed of song in ticks per row
    int pattern_delay_; // pattern delay counter
    Position current_;
    Position next_;
#ifdef FMUSIC_XM_GLOBALVOLSLIDE_ACTIVE
    int global_volume_slide_ = 0; // global mod volume
#endif

    void updateNote();
    void updateTick();

    void clampGlobalVolume() noexcept
    {
        global_volume_ = std::clamp(global_volume_, 0, 64);
    }

    Position tick();

public:
    PlayerState(std::unique_ptr<IPlaybackDriver> driver, std::unique_ptr<Module> module);

    void start()
    {
        mixer_.start();
    }

    void setBPM(unsigned int bpm) noexcept
    {
        mixer_.setBPM(bpm);
    }

    std::unique_ptr<Module> stop()
    {
        mixer_.stop();
        return std::move(module_);
    }

    [[nodiscard]] TimeInfo getTimeInfo() const
    {
        return mixer_.getTimeInfo();
    }
};
