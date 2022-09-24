
#include "channel.h"
#include "xmeffects.h"

namespace
{
    int XMLinearPeriod2Frequency(int per)
    {
        return (int)(8363.0f * powf(2.0f, (6.0f * 12.0f * 16.0f * 4.0f - per) / (float)(12 * 16 * 4)));
    }

    int Period2Frequency(int period)
    {
        return 14317456L / period;
    }
}

void FMUSIC_CHANNEL::processInstrument(const Instrument& instrument) noexcept
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
    int delta;

    switch (instrument.sample_header.vibrato_type)
    {
    case XMInstrumentVibratoType::Sine:
    {
        delta = (int)(sinf((float)ivibpos * (2 * 3.141592f / 256.0f)) * 64.0f);
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

    delta *= instrument.sample_header.vibrato_depth;
    if (instrument.sample_header.vibrato_sweep)
    {
        delta = delta * ivibsweeppos / instrument.sample_header.vibrato_sweep;
    }
    delta /= 64;

    period_delta += delta;

    ivibsweeppos = std::min(ivibsweeppos + 1, int(instrument.sample_header.vibrato_sweep));
    ivibpos = (ivibpos + instrument.sample_header.vibrato_rate) & 255;
#endif	// FMUSIC_XM_INSTRUMENTVIBRATO_ACTIVE
}

void FMUSIC_CHANNEL::reset(int volume, int pan) noexcept
{
    this->volume = volume;
    this->pan = pan;
    volume_envelope.reset(1.0);
    pan_envelope.reset(0.0);

    keyoff = false;
    fadeoutvol = 32768;
    ivibsweeppos = 0;
    ivibpos = 0;

    // retrigger tremolo and vibrato waveforms
    vibrato.reset();
    tremolo.reset();

    tremorpos = 0;								// retrigger tremor count
}

void FMUSIC_CHANNEL::updateFlags(const Sample& sample, int globalvolume, bool linearfrequency) noexcept
{
    FSOUND_CHANNEL& sound_channel = *cptr;

    if (trigger)
    {
        // this swaps between channels to avoid sounds cutting each other off and causing a click
        if (sound_channel.sptr != nullptr)
        {
            int phaseout_sound_channel_number = sound_channel.index + 32;
            FSOUND_CHANNEL& phaseout_sound_channel = FSOUND_Channel[phaseout_sound_channel_number];
            phaseout_sound_channel = sound_channel;
            phaseout_sound_channel.index = phaseout_sound_channel_number;

            // this will cause the copy of the old channel to ramp out nicely.
            phaseout_sound_channel.leftvolume = 0;
            phaseout_sound_channel.rightvolume = 0;
        }

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

    volume = std::clamp(volume, 0, 64);
    voldelta = std::clamp(voldelta, -volume, 64 - volume);
    pan = std::clamp(pan, 0, 255);

    float high_precision_pan = std::clamp(pan + pan_envelope() * (128 - abs(pan - 128)), 0.0f, 255.0f); // 255

    constexpr float norm = 1.0f / 68451041280.0f; // 2^27 (volume normalization) * 255.0 (pan scale) (*2 for safety?!?)

    float high_precision_volume = (volume + voldelta) * fadeoutvol * globalvolume * volume_envelope() * norm;

    sound_channel.leftvolume = high_precision_volume * high_precision_pan;
    sound_channel.rightvolume = high_precision_volume * (255 - high_precision_pan);
    if ((period + period_delta) != 0)
    {
        int freq = std::max(
            (linearfrequency)
            ? XMLinearPeriod2Frequency(period + period_delta)
            : Period2Frequency(period + period_delta),
            100);

        sound_channel.speed = float(freq) / FSOUND_MixRate;
    }
    if (stop)
    {
        sound_channel.mixpos = 0;
        //		ccptr->sptr = nullptr;
        sound_channel.sampleoffset = 0;	// if this channel gets stolen it will be safe
    }
}

void FMUSIC_CHANNEL::processVolumeByte(uint8_t volume_byte) noexcept
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
            vibrato.setup(volumey, 0); // TODO: Check if it's in the correct range (0-15)
            break;
        }
        case 0xb:
        {
            vibrato.setup(0, volumey * 2); // TODO: Check if it's in the correct range (0-15)
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
            portamento.setup(period_target, volumey * 64);
            trigger = false;
            break;
        }
        }
    }
#endif // #define FMUSIC_XM_VOLUMEBYTE_ACTIVE
}

void FMUSIC_CHANNEL::tremor() noexcept
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
