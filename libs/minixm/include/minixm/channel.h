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

#include "envelope.h"
#include "instrument.h"
#include "lfo.h"
#include "mixer.h"
#include "portamento.h"
#include "xmeffects.h"

#include <xmformat/effect.h>

// Channel type - contains information on a mod channel
struct Channel
{
    int index;

    XMNote note; // last note set in channel

    bool trigger;
    bool stop;

    int period; // current mod frequency period for this channel
    int period_delta; // delta for frequency commands.. vibrato/arpeggio etc
    int period_target; // last period set in channel

    int volume; // current mod volume for this channel
    int pan; // current mod pan for this channel
    int volume_delta; // delta for volume commands.. tremolo/tremor etc

    int fade_out_volume; // volume fade out
    bool key_off; // flag whether key off has been hit or not

    int instrument_index; // last instrument set in channel (0-127)
    XMNote real_note; // last real note set in channel
    XMEffect last_effect; // previous row's effect. used to correct tremolo volume

    int8_t fine_tune;

#ifdef FMUSIC_XM_VOLUMEENVELOPE_ACTIVE
    EnvelopeState volume_envelope;
#endif

#ifdef FMUSIC_XM_PANENVELOPE_ACTIVE
    EnvelopeState pan_envelope;
#endif

#ifdef FMUSIC_XM_TREMOLO_ACTIVE
    LFO tremolo; // Tremolo LFO
#endif

#ifdef FMUSIC_XM_TREMOR_ACTIVE
    int tremor_position; // tremor position (XM + S3M)
    int tremor_on; // remembered parameters for tremor (XM + S3M)
    int tremor_off; // remembered parameters for tremor (XM + S3M)
#endif

#if defined(FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_VOLUMESLIDE_ACTIVE)
    int volume_slide; // last volume slide value
#endif

#ifdef FMUSIC_XM_PORTAUP_ACTIVE
    int porta_up; // last porta up value (XM)
#endif

#ifdef FMUSIC_XM_PORTADOWN_ACTIVE
    int porta_down; // last porta down value (XM)
#endif

#ifdef FMUSIC_XM_EXTRAFINEPORTA_ACTIVE
    int extra_fine_porta_down; // last porta down value (XM)
    int extra_fine_porta_up; // last porta up value (XM)
#endif

#ifdef FMUSIC_XM_PANSLIDE_ACTIVE
    int pan_slide; // pan slide value
#endif

#ifdef FMUSIC_XM_MULTIRETRIG_ACTIVE
    XMRetriggerVolumeOperation retrigger_volume_operator; // type of retrigger volume operation
    int retrigger_tick; // last retrig tick count used (XM + S3M)
#endif

#if defined(FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_PORTATO_ACTIVE) || defined(FMUSIC_XM_VOLUMEBYTE_ACTIVE)
    Portamento portamento; // note to porta to and speed
#endif

#if defined(FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_VIBRATO_ACTIVE) || defined(FMUSIC_XM_VOLUMEBYTE_ACTIVE)
    LFO vibrato; // Vibrato LFO
#endif

#ifdef FMUSIC_XM_FINEPORTAUP_ACTIVE
    int fine_porta_up; // parameter for fine porta slide up
#endif

#ifdef FMUSIC_XM_FINEPORTADOWN_ACTIVE
    int fine_porta_down; // parameter for fine porta slide down
#endif

#ifdef FMUSIC_XM_PATTERNLOOP_ACTIVE
    int pattern_loop_row; // where the loop will start
    int pattern_loop_count; // repetition count
#endif

#ifdef FMUSIC_XM_FINEVOLUMESLIDEUP_ACTIVE
    int fine_volume_slide_up; // parameter for fine volume slide up
#endif

#ifdef FMUSIC_XM_FINEVOLUMESLIDEDOWN_ACTIVE
    int fine_volume_slide_down; // parameter for fine volume slide down
#endif

    void processInstrument(Instrument& instrument);
    void reset(int new_volume, int new_pan) noexcept;
    void processVolumeByteNote(int volume_byte) noexcept;
    void processVolumeByteTick(int volume_byte) noexcept;
    void tremor() noexcept;
    void updateVolume() noexcept;

    void setVolSlide(int value) noexcept { if (value) volume_slide = value; }
    void setPanSlide(int value) noexcept { if (value) pan_slide = value; }

    void updatePeriodFromPortamento() noexcept
    {
#if defined(FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_PORTATO_ACTIVE) || defined(FMUSIC_XM_VOLUMEBYTE_ACTIVE)
        period = portamento(period);
#endif
    }

    void sendToMixer(Mixer& mixer, const Instrument& instrument, int global_volume, bool linear_frequency) const;
};
