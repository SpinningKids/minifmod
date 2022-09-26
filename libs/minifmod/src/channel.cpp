
#include "channel.h"
#include "xmeffects.h"

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

void Channel::reset(int volume, int pan) noexcept
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

std::tuple<float, float> Channel::updateVolume(int globalvolume) noexcept
{
    volume = std::clamp(volume, 0, 64);
    voldelta = std::clamp(voldelta, -volume, 64 - volume);
    pan = std::clamp(pan, 0, 255);

    float high_precision_pan = std::clamp(pan + pan_envelope() * (128 - abs(pan - 128)), 0.0f, 255.0f); // 255

    constexpr float norm = 1.0f / 68451041280.0f; // 2^27 (volume normalization) * 255.0 (pan scale) (*2 for safety?!?)

    float high_precision_volume = (volume + voldelta) * fadeoutvol * globalvolume * volume_envelope() * norm;

    return { high_precision_volume, high_precision_pan };
}
