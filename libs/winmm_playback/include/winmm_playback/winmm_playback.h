/******************************************************************************/
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/
/* This library (winmm_playvack) is maintained by Pan/SpinningKids, 2022-2024 */
/******************************************************************************/

#pragma once

#include <thread>

#include <minixm/playback.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

class WindowsPlayback final : public IPlaybackDriver
{
    std::unique_ptr<short[]> buffer_;

    HWAVEOUT wave_out_handle_ = nullptr;

    std::thread software_thread_;
    bool software_thread_exit_ = true; // mixing thread termination flag

public:
    WindowsPlayback() = delete;
    WindowsPlayback(const WindowsPlayback&) = delete;
    WindowsPlayback(WindowsPlayback&&) = delete;
    WindowsPlayback& operator =(const WindowsPlayback&) = delete;
    WindowsPlayback& operator =(WindowsPlayback&&) = delete;

    WindowsPlayback(unsigned int mix_rate, unsigned int buffer_size_ms = 1000, unsigned int latency = 20);

    void start(FillFunction* fill, void* arg) override;

    void stop() override;

    ~WindowsPlayback() override;

    [[nodiscard]] size_t current_block_played() const override;
};
