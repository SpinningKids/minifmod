
#include <algorithm>
#include <minixm/channel.h>

#include <numbers>

#include <minixm/xmeffects.h>

namespace
{
    float XMLinearPeriod2Frequency(int per)
    {
        // From XM.TXT:
        //      Frequency = 8363*2^((6*12*16*4 - Period) / (12*16*4));
        // Simplified by taking log2(8363) inside the power and simplifying
        return exp2f(19.029805f - static_cast<float>(per) / 1536.0f);
    }

    float Period2Frequency(int period)
    {
        // From XM.TXT:
        //      Frequency = 8363*1712/Period;
        return 28634912.0f / static_cast<float>(period);
    }
}

void Channel::processInstrument(const Instrument& instrument) noexcept
{
    //= PROCESS ENVELOPES ==========================================================================
#ifdef FMUSIC_XM_VOLUMEENVELOPE_ACTIVE
    volume_envelope.process(instrument.volume_envelope, instrument.sample_header.volume_envelope_flags, instrument.sample_header.volume_loop_start_index, instrument.sample_header.volume_loop_end_index, instrument.sample_header.volume_sustain_index, keyoff);
#endif
#ifdef FMUSIC_XM_PANENVELOPE_ACTIVE
    pan_envelope.process(instrument.pan_envelope, instrument.sample_header.pan_envelope_flags, instrument.sample_header.pan_loop_start_index, instrument.sample_header.pan_loop_end_index, instrument.sample_header.pan_sustain_index, keyoff);
#endif
    //= PROCESS VOLUME FADEOUT =====================================================================
    if (keyoff)
    {
        fadeoutvol = std::max(fadeoutvol - instrument.sample_header.volume_fadeout, 0);
    }
    //= INSTRUMENT VIBRATO ============================================================================
#ifdef FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE
    int delta = 0;

    switch (instrument.sample_header.vibrato_type)
    {
    case XMInstrumentVibratoType::Sine:
    {
        delta = static_cast<int>(sinf(static_cast<float>(ivibpos) * (2 * std::numbers::pi_v<float> / 256.0f)) * 256.0f);
        break;
    }
    case XMInstrumentVibratoType::Square:
    {
        delta = 256 - (ivibpos & 128) * 4;
        break;
    }
    case XMInstrumentVibratoType::InverseSawTooth:
    {
        delta = (ivibpos & 128) * 4 - (ivibpos + 1);
        break;
    }
    case XMInstrumentVibratoType::SawTooth:
    {
        delta = ivibpos + 1 - (ivibpos & 128) * 4;
        break;
    }
    }

    delta *= instrument.sample_header.vibrato_depth;
    if (instrument.sample_header.vibrato_sweep)
    {
        delta *= ivibsweeppos;
        delta /= instrument.sample_header.vibrato_sweep;
    }
    period_delta += delta / 128;

    ivibsweeppos = std::min(ivibsweeppos + 1, static_cast<int>(instrument.sample_header.vibrato_sweep));
    ivibpos += instrument.sample_header.vibrato_rate;
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

    keyoff = false;
    fadeoutvol = 32768;

#ifdef FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE
    ivibsweeppos = 0;
    ivibpos = 0;
#endif
    // retrigger tremolo and vibrato waveforms
#if defined (FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_VIBRATO_ACTIVE)
    vibrato.reset();
#endif
#ifdef FMUSIC_XM_TREMOLO_ACTIVE
    tremolo.reset();
#endif
#ifdef FMUSIC_XM_TREMOR_ACTIVE
    tremorpos = 0;								// retrigger tremor count
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
    if (tremorpos >= tremoron)
    {
        voldelta = -volume;
    }
    tremorpos++;
    if (tremorpos >= tremoron + tremoroff)
    {
        tremorpos = 0;
    }
#endif
}

void Channel::updateVolume() noexcept
{
    volume = std::clamp(volume, 0, 64);
    voldelta = std::clamp(voldelta, -volume, 64 - volume);
    pan = std::clamp(pan, 0, 255);
}

void Channel::sendToMixer(Mixer& mixer, const Instrument& instrument, int global_volume, bool linear_frequency) const noexcept
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
        sound_channel.sample_offset = 0;	// reset it (in case other samples come in and get corrupted etc)

        // volume ramping
        sound_channel.filtered_left_volume = 0;
        sound_channel.filtered_right_volume = 0;
    }
    constexpr float norm = 1.0f / 68451041280.0f; // 2^27 (volume normalization) * 255.0 (pan scale) (*2 for safety?!?)
    float high_precision_volume = static_cast<float>((volume + voldelta) * fadeoutvol * global_volume) * norm;
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
            linear_frequency
            ? XMLinearPeriod2Frequency(actual_period)
            : Period2Frequency(actual_period),
            100.0f);

        sound_channel.speed = freq / static_cast<float>(mixer.getMixRate());
    }
    if (stop)
    {
        sound_channel.mix_position = 0;
        sound_channel.sample_offset = 0;	// if this channel gets stolen it will be safe
    }
}
