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

#include <algorithm>
#include <minixm/channel.h>

#include <minixm/xmeffects.h>

namespace
{
    float XMLinearPeriod2Frequency(int per) noexcept
    {
        // From XM.TXT:
        //      Frequency = 8363*2^((6*12*16*4 - Period) / (12*16*4));
        // in our case period is multiplied by 2, so everything else should be multiplied as well:
        //      Frequency = 8363*2^((6*1536 - Period) / 1536);
        // which can be simplified to
        //      Frequency = 8363*2^(6 - Period/ 1536);
        // and then separated into
        //      Frequency = 8363 * 2^6 * 2^(-Period/1536);
        // and therefore
        return 535232.f * exp2f(-static_cast<float>(per)/1536.f);
    }

#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
    float Period2Frequency(int period) noexcept
    {
        // From XM.TXT:
        //      Frequency = 8363*1712/Period;
        return 28634912.0f / static_cast<float>(period);
    }
#endif
}

void Channel::processInstrument(Instrument& instrument)
{
    //= PROCESS ENVELOPES ==========================================================================
#ifdef FMUSIC_XM_VOLUMEENVELOPE_ACTIVE
    volume_envelope.process(instrument.volume_envelope, instrument.instrument_sample_header.volume_envelope_flags, instrument.instrument_sample_header.volume_loop_start_index, instrument.instrument_sample_header.volume_loop_end_index, instrument.instrument_sample_header.volume_sustain_index, key_off);
#endif
#ifdef FMUSIC_XM_PANENVELOPE_ACTIVE
    pan_envelope.process(instrument.pan_envelope, instrument.instrument_sample_header.pan_envelope_flags, instrument.instrument_sample_header.pan_loop_start_index, instrument.instrument_sample_header.pan_loop_end_index, instrument.instrument_sample_header.pan_sustain_index, key_off);
#endif
    //= PROCESS VOLUME FADEOUT =====================================================================
    if (key_off)
    {
        fade_out_volume = std::max(fade_out_volume - instrument.instrument_sample_header.volume_fadeout, 0);
    }
    //= INSTRUMENT VIBRATO ============================================================================
#ifdef FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE
    period_delta += instrument.getInstrumentVibratoDelta();
#endif	// FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE
}

void Channel::reset(int new_volume, int new_pan) noexcept
{
    volume = new_volume;
    pan = new_pan;
#ifdef FMUSIC_XM_VOLUMEENVELOPE_ACTIVE
    volume_envelope.reset(1.0);
#endif
#ifdef FMUSIC_XM_PANENVELOPE_ACTIVE
    pan_envelope.reset(0.0);
#endif

    key_off = false;
    fade_out_volume = 32768;

    // retrigger tremolo and vibrato waveforms
#if defined (FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_VIBRATO_ACTIVE)
    vibrato.reset();
#endif
#ifdef FMUSIC_XM_TREMOLO_ACTIVE
    tremolo.reset();
#endif
#ifdef FMUSIC_XM_TREMOR_ACTIVE
    tremor_position = 0;								// retrigger tremor count
#endif
}

void Channel::processVolumeByteNote(int volume_byte) noexcept
{
#ifdef FMUSIC_XM_VOLUMEBYTE_ACTIVE
    // NOTE: Having this makes code actually smaller than using the switch.
    if (volume_byte >= 0x10 && volume_byte <= 0x50)
    {
        volume = volume_byte - 0x10;
    }
    else
    {
        const int volume_y = volume_byte & 0xF;
        switch (volume_byte >> 4)
        {
        case 0x6:
        case 0x8:
        {
            volume -= volume_y;
            break;
        }
        case 0x7:
        case 0x9:
        {
            volume += volume_y;
            break;
        }
        case 0xa:
        {
            vibrato.setSpeed(volume_y);
            break;
        }
        case 0xb:
        {
            vibrato.setDepth(volume_y * 16);
            break;
        }
        case 0xc:
        {
            pan = volume_y * 16;
            break;
        }
        case 0xd:
        {
            pan -= volume_y;
            break;
        }
        case 0xe:
        {
            pan += volume_y;
            break;
        }
        case 0xf:
        {
            portamento.setTarget(period_target);
            portamento.setSpeed(volume_y * 2);
            trigger = false;
            break;
        }
        }
    }
#endif // #define FMUSIC_XM_VOLUMEBYTE_ACTIVE
}

void Channel::processVolumeByteTick(int volume_byte) noexcept
{
#ifdef FMUSIC_XM_VOLUMEBYTE_ACTIVE
    const int volume_y = volume_byte & 0xF;
    switch (volume_byte >> 4)
    {
    case 0x6:
    {
        volume -= volume_y;
        break;
    }
    case 0x7:
    {
        volume += volume_y;
        break;
    }
#ifdef FMUSIC_XM_VIBRATO_ACTIVE
    case 0xb:
    {
        vibrato.setDepth(volume_y * 16);
        period_delta = vibrato();
        vibrato.update();
        break;
    }
#endif
    case 0xd:
    {
        pan -= volume_y;
        break;
    }
    case 0xe:
    {
        pan += volume_y;
        break;
    }
#ifdef FMUSIC_XM_PORTATO_ACTIVE
    case 0xf:
    {
        updatePeriodFromPortamento();
        break;
    }
#endif
    }
#endif
}

void Channel::tremor() noexcept
{
#ifdef FMUSIC_XM_TREMOR_ACTIVE
    if (tremor_position >= tremor_on)
    {
        volume_delta = -volume;
    }
    tremor_position++;
    if (tremor_position >= tremor_on + tremor_off)
    {
        tremor_position = 0;
    }
#endif
}

void Channel::updateVolume() noexcept
{
    volume = std::clamp(volume, 0, 64);
    volume_delta = std::clamp(volume_delta, -volume, 64 - volume);
    pan = std::clamp(pan, 0, 255);
}

void Channel::sendToMixer(Mixer& mixer, const Instrument& instrument, int global_volume, bool linear_frequency) const
{
    MixerChannel& sound_channel = mixer.getChannel(index);
    if (trigger)
    {
        // this swaps between channels to avoid sounds cutting each other off and causing a click
        if (sound_channel.sample_ptr != nullptr)
        {
            MixerChannel& phaseout_sound_channel = mixer.getChannel(index + 32);
            phaseout_sound_channel = sound_channel;

            // this will cause the copy of the old channel to ramp out nicely.
            phaseout_sound_channel.left_volume = 0;
            phaseout_sound_channel.right_volume = 0;
        }

        const Sample& sample = instrument.getSample(note);
        sound_channel.sample_ptr = &sample;

        //==========================================================================================
        // START THE SOUND!
        //==========================================================================================
        if (sound_channel.sample_offset >= sample.header.loop_start + sample.header.loop_length)
        {
            sound_channel.sample_offset = 0;
        }

        sound_channel.mix_position = static_cast<float>(sound_channel.sample_offset);
        sound_channel.speed_direction = MixDir::Forwards;
        sound_channel.sample_offset = 0;	// reset it (in case other samples come in and get corrupted etc...)

        // volume ramping
        sound_channel.filtered_left_volume = 0;
        sound_channel.filtered_right_volume = 0;
    }
    constexpr static float norm = 1.0f / 68451041280.0f; // 2^27 (volume normalization) * 255.0 (pan scale) (*2 for safety?!?)
    float high_precision_volume = static_cast<float>((volume + volume_delta) * fade_out_volume * global_volume) * norm;
#ifdef FMUSIC_XM_VOLUMEENVELOPE_ACTIVE
    high_precision_volume *= volume_envelope();
#endif
    auto high_precision_pan = static_cast<float>(pan);
#ifdef FMUSIC_XM_PANENVELOPE_ACTIVE
    high_precision_pan += pan_envelope() * static_cast<float>(128 - abs(pan - 128));
#endif
    high_precision_pan = std::clamp(high_precision_pan, 0.0f, 255.0f);
    sound_channel.left_volume = high_precision_volume * high_precision_pan;
    sound_channel.right_volume = high_precision_volume * (255 - high_precision_pan);
    if (const int actual_period = period + period_delta; actual_period != 0)
    {
        const float freq = std::max(
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
        (!linear_frequency) ? Period2Frequency(actual_period) :
#endif
            XMLinearPeriod2Frequency(actual_period), 100.0f);

        sound_channel.speed = freq / static_cast<float>(mixer.getMixRate());
    }
    if (stop)
    {
        sound_channel.mix_position = 0;
        sound_channel.sample_offset = 0;	// if this channel gets stolen it will be safe
    }
}
