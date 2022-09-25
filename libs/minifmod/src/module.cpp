/******************************************************************************/
/* MiniFMOD public source code release.                                       */
/* This source is provided as-is.  Firelight Technologies will not support    */
/* or answer questions about the source provided.                             */
/* MiniFMOD Sourcecode is copyright (c) Firelight Technologies, 2000-2004.    */
/* MiniFMOD Sourcecode is in no way representative of FMOD 3 source.          */
/* Firelight Technologies is a registered company.                            */
/* This source must not be redistributed without this notice.                 */
/******************************************************************************/
/* C++ conversion and (heavy) refactoring by Pan/SpinningKids, 2022           */
/******************************************************************************/

#include "module.h"

#include "channel.h"
#include "xmeffects.h"

namespace
{
    constexpr uint8_t FMUSIC_KEYOFF = 97;

#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
    int GetAmigaPeriod(int note)
    {
        return (int)(powf(2.0f, (float)(132 - note) / 12.0f) * 13.375f);
    }

    int FMUSIC_XM_GetAmigaPeriod(int note, int8_t fine_tune) noexcept
    {
        int period = GetAmigaPeriod(note);

        // interpolate for finer tuning
        int direction = (fine_tune > 0) ? 1 : -1;

        period += direction * ((GetAmigaPeriod(note + direction) - period) * fine_tune / 128);

        return period;
    }
#endif // FMUSIC_XM_AMIGAPERIODS_ACTIVE

}

FMUSIC_MODULE::FMUSIC_MODULE(const minifmod::FileAccess& fileAccess, void* fp, SAMPLELOADCALLBACK sampleloadcallback)
{
    fileAccess.seek(fp, 0, SEEK_SET);
    fileAccess.read(&header_, sizeof(header_), fp);

    // seek to patterndata
    fileAccess.seek(fp, 60 + header_.header_size, SEEK_SET);

    // unpack and read patterns
    for (uint16_t pattern_index = 0; pattern_index < header_.patterns_count; pattern_index++)
    {
        XMPatternHeader header;
        fileAccess.read(&header, sizeof(header), fp);

        Pattern& pattern = pattern_[pattern_index];
        pattern.resize(header.rows);

        if (header.packed_pattern_data_size > 0)
        {
            for (uint16_t row = 0; row < header.rows; ++row)
            {
                auto& current_row = pattern[row];

                for (uint16_t channel_index = 0; channel_index < header_.channels_count; channel_index++)
                {
                    unsigned char dat;

                    XMNote& note = current_row[channel_index];

                    fileAccess.read(&dat, 1, fp);
                    if (dat & 0x80)
                    {
                        if (dat & 1)  fileAccess.read(&note.note, 1, fp);
                        if (dat & 2)  fileAccess.read(&note.sample_index, 1, fp);
                        if (dat & 4)  fileAccess.read(&note.volume, 1, fp);
                        if (dat & 8)  fileAccess.read(&note.effect, 1, fp);
                        if (dat & 16) fileAccess.read(&note.effect_parameter, 1, fp);
                    }
                    else
                    {
                        note.note = dat;
                        fileAccess.read(((char*)&note) + 1, sizeof(XMNote) - 1, fp);
                    }

                    if (note.sample_index > 0x80)
                    {
                        note.sample_index = 0;
                    }
                }
            }
        }
    }

    // load instrument information
    for (uint16_t instrument_index = 0; instrument_index < header_.instruments_count; ++instrument_index)
    {
        // point a pointer to that particular instrument
        Instrument& instrument = instrument_[instrument_index];

        int first_sample_offset = fileAccess.tell(fp);
        fileAccess.read(&instrument.header, sizeof(instrument.header), fp);				// instrument size
        first_sample_offset += instrument.header.header_size;

        assert(instrument.header.samples_count <= 16);

        if (instrument.header.samples_count > 0)
        {
            fileAccess.read(&instrument.sample_header, sizeof(instrument.sample_header), fp);

            auto adjust_envelope = [](uint8_t count, const XMEnvelopePoint(&original_points)[12], int offset, float scale, XMEnvelopeFlags flags)
            {
                EnvelopePoints e;
                e.count = (count < 2 || !(flags & XMEnvelopeFlagsOn)) ? 0 : count;
                for (uint8_t i = 0; i < e.count; ++i)
                {
                    e.envelope[i].position = original_points[i].position;
                    e.envelope[i].value = (original_points[i].value - offset) / scale;
                    if (i > 0)
                    {
                        const int tickdiff = e.envelope[i].position - e.envelope[i - 1].position;

                        e.envelope[i - 1].delta = tickdiff ? (e.envelope[i].value - e.envelope[i - 1].value) / tickdiff : 0;
                    }
                }
                if (e.count) {
                    e.envelope[e.count - 1].delta = 0.f;
                }
                return e;
            };
            instrument.volume_envelope = adjust_envelope(instrument.sample_header.volume_envelope_count, instrument.sample_header.volume_envelope, 0, 64, instrument.sample_header.volume_envelope_flags);
            instrument.pan_envelope = adjust_envelope(instrument.sample_header.pan_envelope_count, instrument.sample_header.pan_envelope, 32, 32, instrument.sample_header.pan_envelope_flags);

            // seek to first sample
            fileAccess.seek(fp, first_sample_offset, SEEK_SET);
            for (unsigned int sample_index = 0; sample_index < instrument.header.samples_count; sample_index++)
            {
                XMSampleHeader& sample_header = instrument.sample[sample_index].header;

                fileAccess.read(&sample_header, sizeof(sample_header), fp);

                // type of sample
                if (sample_header.bits16)
                {
                    sample_header.length /= 2;
                    sample_header.loop_start /= 2;
                    sample_header.loop_length /= 2;
                }

                if ((sample_header.loop_mode == XMLoopMode::Off) || (sample_header.length == 0))
                {
                    sample_header.loop_start = 0;
                    sample_header.loop_length = sample_header.length;
                    sample_header.loop_mode = XMLoopMode::Off;
                }

            }

            // Load sample data
            for (unsigned int sample_index = 0; sample_index < instrument.header.samples_count; sample_index++)
            {
                Sample& sample = instrument.sample[sample_index];
                //unsigned int	samplelenbytes = sample.header.length * sample.bits / 8;

                //= ALLOCATE MEMORY FOR THE SAMPLE BUFFER ==============================================

                if (sample.header.length)
                {
                    sample.buff = std::make_unique<int16_t[]>(sample.header.length + 8);

                    if (sampleloadcallback)
                    {
                        sampleloadcallback(sample.buff.get(), sample.header.length, instrument_index, sample_index);
                        fileAccess.seek(fp, sample.header.length * (sample.header.bits16 ? 2 : 1), SEEK_CUR);
                    }
                    else
                    {
                        if (sample.header.bits16)
                        {
                            fileAccess.read(sample.buff.get(), sample.header.length * sizeof(short), fp);
                        }
                        else
                        {
                            auto buff = std::make_unique<int8_t[]>(sample.header.length + 8);
                            fileAccess.read(buff.get(), sample.header.length, fp);
                            for (uint32_t i = 0; i < sample.header.length; i++)
                            {
                                sample.buff[i] = static_cast<int16_t>(buff[i] * 256);
                            }

                            sample.header.bits16 = true;
                        }

                        // DO DELTA CONVERSION
                        int16_t previous_value = 0;
                        for (uint32_t i = 0; i < sample.header.length; i++)
                        {
                            sample.buff[i] = previous_value = static_cast<int16_t>(sample.buff[i] + previous_value);
                        }
                    }

                    // BUGFIX 1.3 - removed click for end of non looping sample (also size optimized a bit)
                    if (sample.header.loop_mode == XMLoopMode::Bidi)
                    {
                        sample.buff[sample.header.loop_start + sample.header.loop_length] = sample.buff[sample.header.loop_start + sample.header.loop_length - 1];// fix it
                    }
                    else if (sample.header.loop_mode == XMLoopMode::Normal)
                    {
                        sample.buff[sample.header.loop_start + sample.header.loop_length] = sample.buff[sample.header.loop_start];// fix it
                    }
                }
            }
        }
        else
        {
            instrument.sample_header = XMInstrumentSampleHeader{};
            fileAccess.seek(fp, first_sample_offset, SEEK_SET);
        }
    }
}

void FMUSIC_MODULE::tick() noexcept
{
    if (tick_ == 0)									// new note
    {
        updateNote();					// Update and play the note
    }
    else
    {
        updateEffects();					// Else update the inbetween row effects
    }

    tick_++;
    if (tick_ >= ticks_per_row_ + pattern_delay_)
    {
        pattern_delay_ = 0;
        tick_ = 0;
    }
}

void FMUSIC_MODULE::updateNote() noexcept
{
    // process any rows commands to set the next order/row
    current_ = next_;

    bool row_set = false;

    // Point our note pointer to the correct pattern buffer, and to the
    // correct offset in this buffer indicated by row and number of channels
    const auto& pattern = pattern_[header_.pattern_order[current_.order]];
    const auto& row = pattern[current_.row];

    // Loop through each channel in the row until we have finished
    for (int channel_index = 0; channel_index < header_.channels_count; channel_index++)
    {
        const XMNote& note = row[channel_index];
        FMUSIC_CHANNEL& channel = FMUSIC_Channel[channel_index];

        const unsigned char paramx = note.effect_parameter >> 4;			// get effect param x
        const unsigned char paramy = note.effect_parameter & 0xF;			// get effect param y
        const int slide = paramx ? paramx : -paramy;

        bool porta = (note.effect == XMEffect::PORTATO || note.effect == XMEffect::PORTATOVOLSLIDE);
        bool valid_note = note.note && note.note != FMUSIC_KEYOFF;

        if (!porta)
        {
            // first store note and instrument number if there was one
            if (note.sample_index)							//  bugfix 3.20 (&& !porta)
            {
                channel.inst = note.sample_index - 1;						// remember the Instrument #
            }

            if (valid_note) //  bugfix 3.20 (&& !porta)
            {
                channel.note = note.note - 1;						// remember the note
            }
        }

        assert(channel.inst < (int)header_.instruments_count);
        const Instrument& instrument = instrument_[channel.inst];

        uint8_t note_sample = instrument.sample_header.note_sample_number[channel.note];

        assert(note_sample < 16);
        const Sample& sample = instrument.sample[note_sample];

        if (valid_note) {
            channel.fine_tune = sample.header.fine_tune;
        }

        if (!porta)
        {
            channel.sptr = &sample;
        }

        int oldvolume = channel.volume;
        int oldperiod = channel.period;
        int oldpan = channel.pan;

        // if there is no more tremolo, set volume to volume + last tremolo delta
        if (channel.recenteffect == XMEffect::TREMOLO && note.effect != XMEffect::TREMOLO)
        {
            channel.volume += channel.voldelta;
        }
        channel.recenteffect = note.effect;

        channel.voldelta = 0;
        channel.trigger = valid_note;
        channel.period_delta = 0;				// this is for vibrato / arpeggio etc
        channel.stop = false;

        //= PROCESS NOTE ===============================================================================
        if (valid_note)
        {
            // get note according to relative note
            channel.realnote = note.note + sample.header.relative_note - 1;

            // get period according to realnote and fine_tune
            if (header_.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY)
            {
                channel.period_target = (10 * 12 * 16 * 4) - (channel.realnote * 16 * 4) - (channel.fine_tune / 2);
            }
            else
            {
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
                channel.period_target = FMUSIC_XM_GetAmigaPeriod(channel.realnote, channel.fine_tune);
#endif
            }

            // frequency only changes if there are no portamento effects
            if (!porta)
            {
                channel.period = channel.period_target;
            }
        }

        //= PROCESS INSTRUMENT NUMBER ==================================================================
        if (note.sample_index)
        {
            channel.reset(sample.header.default_volume, sample.header.default_panning);
        }

        //= PROCESS VOLUME BYTE ========================================================================
        channel.processVolumeByte(note.volume);

        //= PROCESS KEY OFF ============================================================================
        if (note.note == FMUSIC_KEYOFF || note.effect == XMEffect::KEYOFF)
        {
            channel.keyoff = true;
        }

        if (instrument.volume_envelope.count == 0 && channel.keyoff)
        {
            channel.volume_envelope.reset(0.0f);
        }

        //= PROCESS TICK 0 EFFECTS =====================================================================
        switch (note.effect)
        {

#ifdef FMUSIC_XM_PORTAUP_ACTIVE
        case XMEffect::PORTAUP:
        {
            if (note.effect_parameter)
            {
                channel.portaup = note.effect_parameter;
            }
            break;
        }
#endif
#ifdef FMUSIC_XM_PORTADOWN_ACTIVE
        case XMEffect::PORTADOWN:
        {
            if (note.effect_parameter)
            {
                channel.portadown = note.effect_parameter;
            }
            break;
        }
#endif
#ifdef FMUSIC_XM_PORTATO_ACTIVE
        case XMEffect::PORTATO:
        {
            channel.portamento.setup(channel.period_target, note.effect_parameter * 4);
            channel.trigger = false;
            break;
        }
#endif
#ifdef FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE
        case XMEffect::PORTATOVOLSLIDE:
        {
            channel.portamento.setup(channel.period_target);
            if (slide)
            {
                channel.volslide = slide;
            }
            channel.trigger = false;
            break;
        }
#endif
#ifdef FMUSIC_XM_VIBRATO_ACTIVE
        case XMEffect::VIBRATO:
        {
            channel.vibrato.setup(paramx, paramy * 2); // TODO: Check if it's in the correct range (0-15)
            channel.period_delta = channel.vibrato();
            break;
        }
#endif
#ifdef FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE
        case XMEffect::VIBRATOVOLSLIDE:
        {
            if (slide)
            {
                channel.volslide = slide;
            }
            channel.period_delta = channel.vibrato();
            break;								// not processed on tick 0
        }
#endif
#ifdef FMUSIC_XM_TREMOLO_ACTIVE
        case XMEffect::TREMOLO:
        {
            channel.tremolo.setup(paramx, -paramy);
            break;
        }
#endif
#ifdef FMUSIC_XM_SETPANPOSITION_ACTIVE
        case XMEffect::SETPANPOSITION:
        {
            channel.pan = note.effect_parameter;
            break;
        }
#endif
#ifdef FMUSIC_XM_SETSAMPLEOFFSET_ACTIVE
        case XMEffect::SETSAMPLEOFFSET:
        {
            if (note.effect_parameter)
            {
                channel.sampleoffset = note.effect_parameter;
            }

            if (channel.cptr)
            {
                unsigned int offset = channel.sampleoffset * 256;

                if (offset >= sample.header.loop_start + sample.header.loop_length)
                {
                    channel.trigger = false;
                    channel.stop = true;
                }
                else
                {
                    channel.cptr->sampleoffset = offset;
                }
            }
            break;
        }
#endif
#ifdef FMUSIC_XM_VOLUMESLIDE_ACTIVE
        case XMEffect::VOLUMESLIDE:
        {
            if (slide)
            {
                channel.volslide = slide;
            }
            break;
        }
#endif
#ifdef FMUSIC_XM_PATTERNJUMP_ACTIVE
        case XMEffect::PATTERNJUMP: // --- 00 B00 : --- 00 D63 , should put us at ord=0, row=63
        {
            next_.order = note.effect_parameter % header_.song_length;
            next_.row = 0;
            row_set = true;
            break;
        }
#endif
#ifdef FMUSIC_XM_SETVOLUME_ACTIVE
        case XMEffect::SETVOLUME:
        {
            channel.volume = note.effect_parameter;
            break;
        }
#endif
#ifdef FMUSIC_XM_PATTERNBREAK_ACTIVE
        case XMEffect::PATTERNBREAK:
        {
            next_.row = (paramx * 10) + paramy;
            if (next_.row > 63) // NOTE: This seems odd, as the pattern might be longer than 64
            {
                next_.row = 0;
            }
            if (!row_set)
            {
                next_.order = (current_.order + 1) % header_.song_length; // NOTE: shouldn't we go to the restart_position?
            }
            row_set = true;
            break;
        }
#endif
        // extended PT effects
        case XMEffect::SPECIAL:
        {
            switch ((XMSpecialEffect)paramx)
            {
#ifdef FMUSIC_XM_FINEPORTAUP_ACTIVE
            case XMSpecialEffect::FINEPORTAUP:
            {
                if (paramy)
                {
                    channel.fineportaup = paramy;
                }
                channel.period -= channel.fineportaup * 4;
                break;
            }
#endif
#ifdef FMUSIC_XM_FINEPORTADOWN_ACTIVE
            case XMSpecialEffect::FINEPORTADOWN:
            {
                if (paramy)
                {
                    channel.fineportadown = paramy;
                }
                channel.period += channel.fineportadown * 4;
                break;
            }
#endif
#ifdef FMUSIC_XM_GLISSANDO_ACTIVE
            case FMUSIC_XM_GLISSANDO:
            {
                // not implemented
                break;
            }
#endif
#ifdef FMUSIC_XM_SETVIBRATOWAVE_ACTIVE
            case XMSpecialEffect::SETVIBRATOWAVE:
            {
                channel.vibrato.setFlags(paramy);
                break;
            }
#endif
#ifdef FMUSIC_XM_SETFINETUNE_ACTIVE
            case XMSpecialEffect::SETFINETUNE:
            {
                channel.fine_tune = paramy;
                break;
            }
#endif
#ifdef FMUSIC_XM_PATTERNLOOP_ACTIVE
            case XMSpecialEffect::PATTERNLOOP:
            {
                if (paramy == 0)
                {
                    channel.patlooprow = current_.row;
                }
                else
                {
                    if (channel.patloopno > 0)
                    {
                        channel.patloopno = paramy;
                    }
                    else
                    {
                        channel.patloopno--;
                    }
                    if (channel.patloopno)
                    {
                        next_.row = channel.patlooprow;
                        //nextorder = order; // This is not needed, as we initially set order = nextorder;
                        row_set = true;
                    }
                }
                break;
            }
#endif
#ifdef FMUSIC_XM_SETTREMOLOWAVE_ACTIVE
            case XMSpecialEffect::SETTREMOLOWAVE:
            {
                channel.tremolo.setFlags(paramy);
                break;
            }
#endif
#ifdef FMUSIC_XM_SETPANPOSITION16_ACTIVE
            case XMSpecialEffect::SETPANPOSITION16:
            {
                channel.pan = paramy * 16;
                break;
            }
#endif
#ifdef FMUSIC_XM_FINEVOLUMESLIDEUP_ACTIVE
            case XMSpecialEffect::FINEVOLUMESLIDEUP:
            {
                if (paramy)
                {
                    channel.finevslup = paramy;
                }
                channel.volume += channel.finevslup;
                break;
            }
#endif
#ifdef FMUSIC_XM_FINEVOLUMESLIDEDOWN_ACTIVE
            case XMSpecialEffect::FINEVOLUMESLIDEDOWN:
            {
                if (paramy)
                {
                    channel.finevsldown = paramy;
                }
                channel.volume -= channel.finevsldown;
                break;
            }
#endif
#ifdef FMUSIC_XM_NOTEDELAY_ACTIVE
            case XMSpecialEffect::NOTEDELAY:
            {
                channel.volume = oldvolume;
                channel.period = oldperiod;
                channel.pan = oldpan;
                channel.trigger = false;
                break;
            }
#endif
#ifdef FMUSIC_XM_PATTERNDELAY_ACTIVE
            case XMSpecialEffect::PATTERNDELAY:
            {
                pattern_delay_ = paramy;
                pattern_delay_ *= ticks_per_row_;
                break;
            }
#endif
            }
            break;
        }
#ifdef FMUSIC_XM_SETSPED_ACTIVE
        case FMUSIEC_XM_SETSPEED:
        {
            if (note.effect_parameter < 0x20)
            {
                speed = note.effect_parameter;
            }
            else
            {
                mod.setBPM(note.effect_parameter);
            }
            break;
        }
#endif
#ifdef FMUSIC_XM_SETGLOBALVOLUME_ACTIVE
        case XMEffect::SETGLOBALVOLUME:
        {
            global_volume_ = note.effect_parameter;
            break;
        }
#endif
#ifdef FMUSIC_XM_GLOBALVOLSLIDE_ACTIVE
        case XMEffect::GLOBALVOLSLIDE:
        {
            if (slide)
            {
                globalvsl = slide;
            }
            break;
        }
#endif
#ifdef FMUSIC_XM_SETENVELOPEPOS_ACTIVE
        case XMEffect::SETENVELOPEPOS:
        {
            channel.volume_envelope.setPosition(note.effect_parameter);
            break;
        }
#endif
#ifdef FMUSIC_XM_PANSLIDE_ACTIVE
        case XMEffect::PANSLIDE:
        {
            if (slide)
            {
                channel.panslide = slide;
            }
            break;
        }
#endif
#ifdef FMUSIC_XM_MULTIRETRIG_ACTIVE
        case XMEffect::MULTIRETRIG:
        {
            if (note.effect_parameter)
            {
                channel.retrigx = paramx;
                channel.retrigy = paramy;
            }
            break;
        }
#endif
#ifdef FMUSIC_XM_TREMOR_ACTIVE
        case XMEffect::TREMOR:
        {
            if (note.effect_parameter)
            {
                channel.tremoron = (paramx + 1);
                channel.tremoroff = (paramy + 1);
            }
            channel.tremor();
            break;
        }
#endif
#ifdef FMUSIC_XM_EXTRAFINEPORTA_ACTIVE
        case XMEffect::EXTRAFINEPORTA:
        {
            switch (paramx)
            {
            case 1:
            {
                if (paramy)
                {
                    channel.xtraportaup = paramy;
                }
                channel.period -= channel.xtraportaup;
                break;
            }
            case 2:
            {
                if (paramy)
                {
                    channel.xtraportadown = paramy;
                }
                channel.period += channel.xtraportadown;
                break;
            }
            }
            break;
        }
#endif
        }

        channel.processInstrument(instrument);

        clampGlobalVolume();
        channel.updateFlags(sample, global_volume_, header_.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY);

        // if there were no row commands
        if (!row_set)
        {
            if (current_.row + 1 < pattern.size())	// if end of pattern TODO: Fix signed/unsigned comparison
            {
                next_.row = current_.row + 1;
            }
            else
            {
                next_.row = 0;						// start at top of pattn
                next_.order = current_.order + 1 < header_.song_length ? current_.order + 1 : header_.restart_position;
            }
        }
    }
}

void FMUSIC_MODULE::updateEffects() noexcept
{
    // Point our note pointer to the correct pattern buffer, and to the
    // correct offset in this buffer indicated by row and number of channels
    const auto& pattern = pattern_[header_.pattern_order[current_.order]];
    const auto& row = pattern[current_.row];

    // Loop through each channel in the row until we have finished
    for (int channel_index = 0; channel_index < header_.channels_count; channel_index++)
    {
        const XMNote& note = row[channel_index];
        FMUSIC_CHANNEL& channel = FMUSIC_Channel[channel_index];

        const unsigned char paramx = note.effect_parameter >> 4;			// get effect param x
        const unsigned char paramy = note.effect_parameter & 0xF;			// get effect param y

        assert(channel.inst < (int)header_.instruments_count);
        const Instrument& instrument = instrument_[channel.inst];

        uint8_t note_sample = instrument.sample_header.note_sample_number[channel.note];

        assert(note_sample < 16);
        const Sample& sample = instrument.sample[note_sample];

        channel.voldelta = 0;
        channel.trigger = false;
        channel.period_delta = 0;				// this is for vibrato / arpeggio etc
        channel.stop = false;

#ifdef FMUSIC_XM_VOLUMEBYTE_ACTIVE
        int volumey = (note.volume & 0xF);
        switch (note.volume >> 4)
        {
            //			case 0x0:
            //			case 0x1:
            //			case 0x2:
            //			case 0x3:
            //			case 0x4:
            //			case 0x5:
            //				break;
        case 0x6:
        {
            channel.volume -= volumey;
            break;
        }
        case 0x7:
        {
            channel.volume += volumey;
            break;
        }
#ifdef FMUSIC_XM_VIBRATO_ACTIVE
        case 0xb:
        {
            channel.vibrato.setup(0, volumey * 2); // TODO: Check if it's in the correct range (0-15)
            channel.period_delta = channel.vibrato();
            channel.vibrato.update();
            break;
        }
#endif
        case 0xd:
        {
            channel.pan -= volumey;
            break;
        }
        case 0xe:
        {
            channel.pan += volumey;
            break;
        }
#ifdef FMUSIC_XM_PORTATO_ACTIVE
        case 0xf:
        {
            channel.period = channel.portamento(channel.period);
            break;
        }
#endif
        }
#endif


        //= PROCESS TICK N != 0 EFFECTS =====================================================================
        switch (note.effect)
        {
#ifdef FMUSIC_XM_ARPEGGIO_ACTIVE
        case XMEffect::ARPEGGIO:
        {
            int v;
            switch (tick_ % 3)
            {
            case 0:
                v = 0;
                break;
            case 1:
                v = paramx;
                break;
            case 2:
                v = paramy;
                break;
            }
            if (header_.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY)
            {
                channel.period_delta = v * 64;
            }
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
            else
            {
                channel.period_delta = FMUSIC_XM_GetAmigaPeriod(channel.realnote + v, channel.fine_tune) -
                    FMUSIC_XM_GetAmigaPeriod(channel.realnote, channel.fine_tune);
            }
#endif
            break;
        }
#endif
#ifdef FMUSIC_XM_PORTAUP_ACTIVE
        case XMEffect::PORTAUP:
        {
            channel.period_delta = 0;
            channel.period = std::max(channel.period - (channel.portaup * 4), 56); // subtract period and stop at B#8
            break;
        }
#endif
#ifdef FMUSIC_XM_PORTADOWN_ACTIVE
        case XMEffect::PORTADOWN:
        {
            channel.period_delta = 0;
            channel.period += channel.portadown * 4; // subtract period
            break;
        }
#endif
#if defined(FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_PORTATO_ACTIVE)
#ifdef FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE
        case XMEffect::PORTATOVOLSLIDE:
            channel.volume += channel.volslide;
#ifdef FMUSIC_XM_PORTATO_ACTIVE
            [[fallthrough]];
#endif
#endif
#ifdef FMUSIC_XM_PORTATO_ACTIVE
        case XMEffect::PORTATO:
#endif
            channel.period_delta = 0;
            channel.period = channel.portamento(channel.period);
            break;
#endif
#if defined (FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_VIBRATO_ACTIVE)
#ifdef FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE
        case XMEffect::VIBRATOVOLSLIDE:
            channel.volume += channel.volslide;
#ifdef FMUSIC_XM_VIBRATO_ACTIVE
            [[fallthrough]];
#endif
#endif
#ifdef FMUSIC_XM_VIBRATO_ACTIVE
        case XMEffect::VIBRATO:
#endif
            channel.period_delta = channel.vibrato();
            channel.vibrato.update();
            break;
#endif
#ifdef FMUSIC_XM_TREMOLO_ACTIVE
        case XMEffect::TREMOLO:
        {
            channel.voldelta = channel.tremolo();
            channel.tremolo.update();
            break;
        }
#endif
#ifdef FMUSIC_XM_VOLUMESLIDE_ACTIVE
        case XMEffect::VOLUMESLIDE:
        {
            channel.volume += channel.volslide;
            break;
        }
#endif
        // extended PT effects
        case XMEffect::SPECIAL:
        {
            switch ((XMSpecialEffect)paramx)
            {
#ifdef FMUSIC_XM_RETRIG_ACTIVE
            case XMSpecialEffect::RETRIG:
            {
                if (paramy && tick_ % paramy == 0)
                {
                    channel.trigger = true;
                }
                break;
            }
#endif
#ifdef FMUSIC_XM_NOTECUT_ACTIVE
            case XMSpecialEffect::NOTECUT:
            {
                if (tick_ == paramy)
                {
                    channel.volume = 0;
                }
                break;
            }
#endif
#ifdef FMUSIC_XM_NOTEDELAY_ACTIVE
            case XMSpecialEffect::NOTEDELAY:
            {
                if (tick_ == paramy)
                {
                    //= PROCESS INSTRUMENT NUMBER ==================================================================
                    channel.reset(sample.header.default_volume, sample.header.default_panning);
                    channel.period = channel.period_target;
                    channel.processVolumeByte(note.volume);
                    channel.trigger = true;
                }
                break;
            }
#endif
            }
            break;
        }
#ifdef FMUSIC_XM_GLOBALVOLSLIDE_ACTIVE
        case XMEffect::GLOBALVOLSLIDE:
        {
            global_volume_ += globalvsl;
            break;
        }
#endif
#ifdef FMUSIC_XM_PANSLIDE_ACTIVE
        case XMEffect::PANSLIDE:
        {
            channel.pan += channel.panslide;
            break;
        }
#endif
#ifdef FMUSIC_XM_MULTIRETRIG_ACTIVE
        case XMEffect::MULTIRETRIG:
        {
            if (channel.retrigy && !(tick_ % channel.retrigy))
            {
                switch (channel.retrigx)
                {
                case 0:
                {
                    break;
                }
                case 1:
                {
                    channel.volume--;
                    break;
                }
                case 2:
                {
                    channel.volume -= 2;
                    break;
                }
                case 3:
                {
                    channel.volume -= 4;
                    break;
                }
                case 4:
                {
                    channel.volume -= 8;
                    break;
                }
                case 5:
                {
                    channel.volume -= 16;
                    break;
                }
                case 6:
                {
                    channel.volume = channel.volume * 2 / 3;
                    break;
                }
                case 7:
                {
                    channel.volume /= 2;
                    break;
                }
                case 8:
                {
                    // ?
                    break;
                }
                case 9:
                {
                    channel.volume++;
                    break;
                }
                case 0xA:
                {
                    channel.volume += 2;
                    break;
                }
                case 0xB:
                {
                    channel.volume += 4;
                    break;
                }
                case 0xC:
                {
                    channel.volume += 8;
                    break;
                }
                case 0xD:
                {
                    channel.volume += 16;
                    break;
                }
                case 0xE:
                {
                    channel.volume = channel.volume * 3 / 2;
                    break;
                }
                case 0xF:
                {
                    channel.volume *= 2;
                    break;
                }
                }
                channel.trigger = true;
            }
            break;
        }
#endif
#ifdef FMUSIC_XM_TREMOR_ACTIVE
        case XMEffect::TREMOR:
        {
            channel.tremor();
            break;
        }
#endif
        }

        channel.processInstrument(instrument);

        clampGlobalVolume();
        channel.updateFlags(sample, global_volume_, header_.flags& FMUSIC_XMFLAGS_LINEARFREQUENCY);
    }
}

void FMUSIC_MODULE::reset() noexcept
{
    global_volume_ = 64;
    ticks_per_row_ = (int)header_.default_tempo;
    current_.row = 0;
    current_.order = 0;
    next_.row = 0;
    next_.order = 0;
    mixer_samples_left_ = 0;
    tick_ = 0;
    pattern_delay_ = 0;
    samples_mixed_ = 0;

    setBPM(header_.default_bpm);

    memset(FMUSIC_Channel, 0, header_.channels_count * sizeof(FMUSIC_CHANNEL));
    //	memset(FSOUND_Channel, 0, 64 * sizeof(FSOUND_CHANNEL));

    for (uint16_t channel_index = 0; channel_index < header_.channels_count; channel_index++)
    {
        FMUSIC_Channel[channel_index].cptr = &FSOUND_Channel[channel_index];
    }

}
