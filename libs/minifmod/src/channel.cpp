
#include "channel.h"
#include "xmeffects.h"

namespace
{
    float XMLinearPeriod2Frequency(float per)
    {
        // From XM.TXT:
        //      Frequency = 8363*2^((6*12*16*4 - Period) / (12*16*4));
        // Simplified by taking log2(8363) inside the power and simplifying
        return powf(2.0f, 19.029805f - per / 768.0f);
        //return (int)(8363.0f * powf(2.0f, (6.0f * 12.0f * 16.0f * 4.0f - per) / (float)(12 * 16 * 4)));
    }

    float Period2Frequency(float period)
    {
        // From XM.TXT:
        //      Frequency = 8363*1712/Period;
        return 14317456.0f / period;
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
    float delta;

    switch (instrument.sample_header.vibrato_type)
    {
    case XMInstrumentVibratoType::Sine:
    {
        delta = sinf((float)ivibpos * (2 * 3.141592f / 256.0f))*64.f;
        break;
    }
    case XMInstrumentVibratoType::Square:
    {
        delta = 64 - (ivibpos & 128);
        break;
    }
    case XMInstrumentVibratoType::InverseSawTooth:
    {
        delta = (ivibpos & 128) - ((ivibpos + 1) / 4);
        break;
    }
    case XMInstrumentVibratoType::SawTooth:
    {
        delta = ((ivibpos + 1) / 4) - (ivibpos & 128);
        break;
    }
    }

    delta *= instrument.sample_header.vibrato_depth / 64.0f;
    if (instrument.sample_header.vibrato_sweep)
    {
        delta *= float(ivibsweeppos) / instrument.sample_header.vibrato_sweep;
    }

    period_delta += delta;

    ivibsweeppos = std::min(ivibsweeppos + 1, int(instrument.sample_header.vibrato_sweep));
    ivibpos += instrument.sample_header.vibrato_rate;
#endif	// FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE
}

void Channel::reset(int volume, int pan) noexcept
{
    this->volume = volume;
    this->pan = pan;
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

void Channel::processVolumeByte(uint8_t volume_byte) noexcept
{
#ifdef FMUSIC_XM_VOLUMEBYTE_ACTIVE
    // NOTE: Having this if makes code actually smaller than using the switch.
    if (volume_byte >= 0x10 && volume_byte <= 0x50)
    {
        volume = volume_byte - 0x10;
    }
    else
    {
        int volumey = (volume_byte & 0xF);
        switch (volume_byte >> 4)
        {
        case 0x6:
        case 0x8:
        {
            volume -= volumey;
            break;
        }
        case 0x7:
        case 0x9:
        {
            volume += volumey;
            break;
        }
        case 0xa:
        {
            if (volumey)
            {
                vibrato.setSpeed(volumey);
            }
            break;
        }
        case 0xb:
        {
            if (volumey)
            {
                vibrato.setDepth(volumey * 8);
            }
            break;
        }
        case 0xc:
        {
            pan = volumey * 16;
            break;
        }
        case 0xd:
        {
            pan -= volumey;
            break;
        }
        case 0xe:
        {
            pan += volumey;
            break;
        }
        case 0xf:
        {
            portamento.setTarget(period_target);
            if (volumey)
            {
                portamento.setSpeed(volumey * 64);
            }
            trigger = false;
            break;
        }
        }
    }
#endif // #define FMUSIC_XM_VOLUMEBYTE_ACTIVE
}

void Channel::tremor() noexcept
{
#ifdef FMUSIC_XM_TREMOR_ACTIVE
    if (tremorpos >= tremoron)
    {
        voldelta = -volume;
    }
    tremorpos++;
    if (tremorpos >= (tremoron + tremoroff))
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

void Channel::sendToMixer(Mixer& mixer, const Instrument& instrument, int globalvolume, bool linearfrequency) const noexcept
{
    MixerChannel& sound_channel = mixer.getChannel(index);
    if (trigger)
    {
        // this swaps between channels to avoid sounds cutting each other off and causing a click
        if (sound_channel.sptr != nullptr)
        {
            MixerChannel& phaseout_sound_channel = mixer.getChannel(index + 32);
            phaseout_sound_channel = sound_channel;

            // this will cause the copy of the old channel to ramp out nicely.
            phaseout_sound_channel.leftvolume = 0;
            phaseout_sound_channel.rightvolume = 0;
        }

        const Sample& sample = instrument.getSample(note);
        sound_channel.sptr = &sample;

        //==========================================================================================
        // START THE SOUND!
        //==========================================================================================
        if (sound_channel.sampleoffset >= sample.header.loop_start + sample.header.loop_length)
        {
            sound_channel.sampleoffset = 0;
        }

        sound_channel.mixpos = (float)sound_channel.sampleoffset;
        sound_channel.speeddir = MixDir::Forwards;
        sound_channel.sampleoffset = 0;	// reset it (in case other samples come in and get corrupted etc)

        // volume ramping
        sound_channel.filtered_leftvolume = 0;
        sound_channel.filtered_rightvolume = 0;
    }
    constexpr float norm = 1.0f / 68451041280.0f; // 2^27 (volume normalization) * 255.0 (pan scale) (*2 for safety?!?)
    float high_precision_volume = (volume + voldelta) * fadeoutvol * globalvolume * norm;
#ifdef FMUSIC_XM_VOLUMEENVELOPE_ACTIVE
    high_precision_volume *= volume_envelope();
#endif
    float high_precision_pan = pan;
#ifdef FMUSIC_XM_PANENVELOPE_ACTIVE
    high_precision_pan += pan_envelope() * (128 - abs(pan - 128));
#endif
    high_precision_pan = std::clamp(high_precision_pan, 0.0f, 255.0f);
    sound_channel.leftvolume = high_precision_volume * high_precision_pan;
    sound_channel.rightvolume = high_precision_volume * (255 - high_precision_pan);
    const float actual_period = period + period_delta;
    if (actual_period != 0)
    {
        const float freq = std::max(
            (linearfrequency)
            ? XMLinearPeriod2Frequency(actual_period)
            : Period2Frequency(actual_period),
            100.0f);

        sound_channel.speed = freq / mixer.getMixRate();
    }
    if (stop)
    {
        sound_channel.mixpos = 0;
        sound_channel.sampleoffset = 0;	// if this channel gets stolen it will be safe
    }
}
