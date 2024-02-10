
#include "player_state.h"

#include "xmeffects.h"

namespace
{
    int GetXMLinearPeriodFinetuned(XMNote note, int fine_tune)
    {
        return 121*128 - note.value*128 - fine_tune;
    }

#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
    int GetAmigaPeriod(XMNote note)
    {
        return static_cast<int>(exp2f(15.824799333333333333333333333333f - static_cast<float>(note.value)));
    }

    int GetAmigaPeriodFinetuned(XMNote note, int fine_tune) noexcept
    {
        int period = GetAmigaPeriod(note);

        // interpolate for finer tuning
        const int direction = (fine_tune > 0) ? 1 : -1;

        period += direction * ((GetAmigaPeriod(note + direction) - period) * fine_tune / 128);

        return period;
    }
#endif

    int GetPeriodFinetuned(XMNote note, int fine_tune, bool linear)
    {
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
        if (!linear)
            return GetAmigaPeriodFinetuned(note, fine_tune);
#endif
        return GetXMLinearPeriodFinetuned(note, fine_tune);
    }

    int GetPeriodDeltaFinetuned(XMNote note, int delta, int fine_tune, bool linear)
    {
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
        if (!linear)
            return GetAmigaPeriodFinetuned(note + delta, fine_tune) - GetAmigaPeriodFinetuned(note, fine_tune);
#endif
        return delta * 128;
    }
}

Position PlayerState::tick() noexcept
{
    if (tick_ == 0)									// new note
    {
        updateNote();					// Update and play the note
    }
    else
    {
        updateTick();					// Else update the inbetween row effects
    }

    clampGlobalVolume();
    for (int channel_index = 0; channel_index < module_->header_.channels_count; channel_index++)
    {
        Channel& channel = channels_[channel_index];
        channel.updateVolume();
        const Instrument& instrument = module_->getInstrument(channel.inst);
        channel.processInstrument(instrument);
        channel.sendToMixer(mixer_, instrument, global_volume_, module_->header_.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY);
    }

    tick_++;
    if (tick_ >= ticks_per_row_ + pattern_delay_)
    {
        pattern_delay_ = 0;
        tick_ = 0;
    }
    return current_;
}

void PlayerState::updateNote() noexcept
{
    // process any rows commands to set the next order/row
    current_ = next_;

    bool row_set = false;

    // Point our note pointer to the correct pattern buffer, and to the
    // correct offset in this buffer indicated by row and number of channels
    const auto& pattern = module_->pattern_[module_->header_.pattern_order[current_.order]];
    const auto& row = pattern[current_.row];

    // Loop through each channel in the row until we have finished
    for (int channel_index = 0; channel_index < module_->header_.channels_count; channel_index++)
    {
        const auto& [note, sample_index, volume, effect, effect_parameter] = row[channel_index];
        Channel& channel = channels_[channel_index];

        const int paramx = effect_parameter >> 4;			// get effect param x
        const int paramy = effect_parameter & 0xF;			// get effect param y
        const int slide = paramx ? paramx : -paramy;

        const bool porta = effect == XMEffect::PORTATO || effect == XMEffect::PORTATOVOLSLIDE;
        const bool valid_note = note.isValid();

        if (!porta)
        {
            // first store note and instrument number if there was one
            if (sample_index)							//  bugfix 3.20 (&& !porta)
            {
                channel.inst = sample_index - 1;						// remember the Instrument #
            }

            if (valid_note) //  bugfix 3.20 (&& !porta)
            {
                channel.note = note;						// remember the note
            }
        }

        const Instrument& instrument = module_->getInstrument(channel.inst);
        const XMSampleHeader& sample_header = instrument.getSample(channel.note).header;

        if (valid_note) {
            channel.fine_tune = sample_header.fine_tune;
        }

        const int old_volume = channel.volume;
        const int old_period = channel.period;
        const int old_pan = channel.pan;

        // if there is no more tremolo, set volume to volume + last tremolo delta
        if (channel.recenteffect == XMEffect::TREMOLO && effect != XMEffect::TREMOLO)
        {
            channel.volume += channel.voldelta;
        }
        channel.recenteffect = effect;

        channel.voldelta = 0;
        channel.trigger = valid_note;
        channel.period_delta = 0;				// this is for vibrato / arpeggio etc
        channel.stop = false;

        //= PROCESS NOTE ===============================================================================
        if (valid_note)
        {
            // get note according to relative note
            channel.realnote = note + sample_header.relative_note;

            // get period according to realnote and fine_tune
            channel.period_target = GetPeriodFinetuned(channel.realnote, channel.fine_tune, module_->header_.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY);

            // frequency only changes if there are no portamento effects
            if (!porta)
            {
                channel.period = channel.period_target;
            }
        }

        //= PROCESS INSTRUMENT NUMBER ==================================================================
        if (sample_index)
        {
            channel.reset(sample_header.default_volume, sample_header.default_panning);
        }

        //= PROCESS VOLUME BYTE ========================================================================
        channel.processVolumeByteNote(volume);

        //= PROCESS KEY OFF ============================================================================
        if (note.isKeyOff() || effect == XMEffect::KEYOFF)
        {
            channel.keyoff = true;
        }

#ifdef FMUSIC_XM_VOLUMEENVELOPE_ACTIVE
        if (instrument.volume_envelope.count == 0 && channel.keyoff)
        {
            channel.volume_envelope.reset(0.0f);
        }
#endif

        //= PROCESS TICK 0 EFFECTS =====================================================================
        switch (effect)
        {
#ifdef FMUSIC_XM_PORTAUP_ACTIVE
            case XMEffect::PORTAUP:
            {
                if (effect_parameter)
                {
                    channel.portaup = effect_parameter;
                }
                break;
            }
#endif
#ifdef FMUSIC_XM_PORTADOWN_ACTIVE
            case XMEffect::PORTADOWN:
            {
                if (effect_parameter)
                {
                    channel.portadown = effect_parameter;
                }
                break;
            }
#endif
#ifdef FMUSIC_XM_PORTATO_ACTIVE
            case XMEffect::PORTATO:
            {
                channel.portamento.setTarget(channel.period_target);
                channel.portamento.setSpeed(effect_parameter * 8);
                channel.trigger = false;
                break;
            }
#endif
#ifdef FMUSIC_XM_VIBRATO_ACTIVE
            case XMEffect::VIBRATO:
            {
                channel.vibrato.setSpeed(paramx);
                channel.vibrato.setDepth(paramy * 16);
                channel.period_delta = channel.vibrato();
                break;
            }
#endif
#ifdef FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE
            case XMEffect::PORTATOVOLSLIDE:
            {
                channel.portamento.setTarget(channel.period_target);
                channel.setVolSlide(slide);
                channel.trigger = false;
                break;
            }
#endif
#ifdef FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE
            case XMEffect::VIBRATOVOLSLIDE:
            {
                channel.setVolSlide(slide);
                channel.period_delta = channel.vibrato();
                break;								// not processed on tick 0
            }
#endif
#ifdef FMUSIC_XM_TREMOLO_ACTIVE
            case XMEffect::TREMOLO:
            {
                channel.tremolo.setSpeed(paramx);
                channel.tremolo.setDepth(-paramy * 4);
                break;
            }
#endif
#ifdef FMUSIC_XM_SETPANPOSITION_ACTIVE
            case XMEffect::SETPANPOSITION:
            {
                channel.pan = effect_parameter;
                break;
            }
#endif
#ifdef FMUSIC_XM_SETSAMPLEOFFSET_ACTIVE
            case XMEffect::SETSAMPLEOFFSET:
            {
                channel.setSampleOffset(effect_parameter * 256);
                if (channel.sampleoffset < sample_header.loop_start + sample_header.loop_length)
                {
                    mixer_.getChannel(channel.index).sample_offset = channel.sampleoffset;
                }
                else
                {
                    channel.trigger = false;
                    channel.stop = true;
                }
                break;
            }
#endif
#ifdef FMUSIC_XM_VOLUMESLIDE_ACTIVE
            case XMEffect::VOLUMESLIDE:
            {
                channel.setVolSlide(slide);
                break;
            }
#endif
#ifdef FMUSIC_XM_PATTERNJUMP_ACTIVE
            case XMEffect::PATTERNJUMP: // --- 00 B00 : --- 00 D63 , should put us at ord=0, row=63
            {
                next_.order = effect_parameter % module_->header_.song_length;
                next_.row = 0;
                row_set = true;
                break;
            }
#endif
#ifdef FMUSIC_XM_SETVOLUME_ACTIVE
            case XMEffect::SETVOLUME:
            {
                channel.volume = effect_parameter;
                break;
            }
#endif
#ifdef FMUSIC_XM_PATTERNBREAK_ACTIVE
            case XMEffect::PATTERNBREAK:
            {
                next_.row = paramx * 10 + paramy;
                if (next_.row > 63) // NOTE: This seems odd, as the pattern might be longer than 64
                {
                    next_.row = 0;
                }
                if (!row_set)
                {
                    next_.order = (current_.order + 1) % module_->header_.song_length; // NOTE: shouldn't we go to the restart_position?
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
                        channel.period -= channel.fineportaup * 8;
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
                        channel.period += channel.fineportadown * 8;
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
#if defined(FMUSIC_XM_SETVIBRATOWAVE_ACTIVE) && (defined (FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_VIBRATO_ACTIVE) || defined(FMUSIC_XM_VOLUMEBYTE_ACTIVE))
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
#if defined(FMUSIC_XM_TREMOLO_ACTIVE) && defined(FMUSIC_XM_SETTREMOLOWAVE_ACTIVE)
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
                        channel.volume = old_volume;
                        channel.period = old_period;
                        channel.pan = old_pan;
                        channel.trigger = false;
                        break;
                    }
#endif
#ifdef FMUSIC_XM_PATTERNDELAY_ACTIVE
                    case XMSpecialEffect::PATTERNDELAY:
                    {
                        pattern_delay_ = ticks_per_row_ * paramy;
                        break;
                    }
#endif
                }
                break;
            }
#ifdef FMUSIC_XM_SETSPEED_ACTIVE
            case XMEffect::SETSPEED:
            {
                if (effect_parameter < 0x20)
                {
                    ticks_per_row_ = effect_parameter;
                }
                else
                {
                    setBPM(effect_parameter);
                }
                break;
            }
#endif
#ifdef FMUSIC_XM_SETGLOBALVOLUME_ACTIVE
            case XMEffect::SETGLOBALVOLUME:
            {
                global_volume_ = effect_parameter;
                break;
            }
#endif
#ifdef FMUSIC_XM_GLOBALVOLSLIDE_ACTIVE
            case XMEffect::GLOBALVOLSLIDE:
            {
                if (slide)
                {
                    global_volume_slide_ = slide;
                }
                break;
            }
#endif
#if defined(FMUSIC_XM_SETENVELOPEPOS_ACTIVE) && defined(FMUSIC_XM_VOLUMEENVELOPE_ACTIVE)
            case XMEffect::SETENVELOPEPOS:
            {
                channel.volume_envelope.setPosition(effect_parameter);
                break;
            }
#endif
#ifdef FMUSIC_XM_PANSLIDE_ACTIVE
            case XMEffect::PANSLIDE:
            {
                channel.setPanSlide(slide);
                break;
            }
#endif
#ifdef FMUSIC_XM_MULTIRETRIG_ACTIVE
            case XMEffect::MULTIRETRIG:
            {
                if (effect_parameter)
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
                if (effect_parameter)
                {
                    channel.tremoron = paramx + 1;
                    channel.tremoroff = paramy + 1;
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
                        channel.period -= channel.xtraportaup * 2;
                        break;
                    }
                    case 2:
                    {
                        if (paramy)
                        {
                            channel.xtraportadown = paramy;
                        }
                        channel.period += channel.xtraportadown * 2;
                        break;
                    }
                }
                break;
            }
#endif
        }
    }

    // if there were no row commands
    if (!row_set)
    {
        next_.row = current_.row + 1;
        if (next_.row >= pattern.size())	// if end of pattern
        {
            next_.row = 0;						// start at top of pattn
            next_.order = current_.order + 1;
            if (next_.order >= module_->header_.song_length)
            {
                next_.order = module_->header_.restart_position;
            }
        }
    }
}

void PlayerState::updateTick() noexcept
{
    // Point our note pointer to the correct pattern buffer, and to the
    // correct offset in this buffer indicated by row and number of channels
    const auto& pattern = module_->pattern_[module_->header_.pattern_order[current_.order]];
    const auto& row = pattern[current_.row];

    // Loop through each channel in the row until we have finished
    for (int channel_index = 0; channel_index < module_->header_.channels_count; channel_index++)
    {
        const auto& [note, sample_index, volume, effect, effect_parameter] = row[channel_index];
        Channel& channel = channels_[channel_index];

        const int paramx = effect_parameter >> 4;			// get effect param x
        const int paramy = effect_parameter & 0xF;			// get effect param y

        channel.voldelta = 0;
        channel.trigger = false;
        channel.period_delta = 0;				// this is for vibrato / arpeggio etc
        channel.stop = false;

        //= PROCESS VOLUME BYTE ========================================================================
        channel.processVolumeByteTick(volume);

        //= PROCESS TICK N != 0 EFFECTS =====================================================================
        switch (effect)
        {
#ifdef FMUSIC_XM_ARPEGGIO_ACTIVE
            case XMEffect::ARPEGGIO:
            {
                int v = 0;
                switch (tick_ % 3)
                {
                    case 1:
                        v = paramx;
                        break;
                    case 2:
                        v = paramy;
                        break;
                }
                channel.period_delta = GetPeriodDeltaFinetuned(channel.realnote, v, channel.fine_tune, module_->header_.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY);
                break;
            }
#endif
#ifdef FMUSIC_XM_PORTAUP_ACTIVE
            case XMEffect::PORTAUP:
            {
                channel.period_delta = 0;
                channel.period = std::max(channel.period - (channel.portaup * 8), 112); // subtract period and stop at B#8
                break;
            }
#endif
#ifdef FMUSIC_XM_PORTADOWN_ACTIVE
            case XMEffect::PORTADOWN:
            {
                channel.period_delta = 0;
                channel.period += channel.portadown * 8; // subtract period
                break;
            }
#endif
#ifdef FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE
            case XMEffect::PORTATOVOLSLIDE:
            {
                channel.volume += channel.volslide;
#endif
#if defined(FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE) && defined(FMUSIC_XM_PORTATO_ACTIVE)
            }
            [[fallthrough]];
#endif
#ifdef FMUSIC_XM_PORTATO_ACTIVE
            case XMEffect::PORTATO:
            {
#endif
#if defined(FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_PORTATO_ACTIVE)
                channel.period_delta = 0;
                channel.updatePeriodFromPortamento();
                break;
            }
#endif
#ifdef FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE
            case XMEffect::VIBRATOVOLSLIDE:
            {
                channel.volume += channel.volslide;
#endif
#if defined (FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE) && defined(FMUSIC_XM_VIBRATO_ACTIVE)
            }
            [[fallthrough]];
#endif
#ifdef FMUSIC_XM_VIBRATO_ACTIVE
            case XMEffect::VIBRATO:
            {
#endif
#if defined (FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE) || defined(FMUSIC_XM_VIBRATO_ACTIVE)
                channel.period_delta = channel.vibrato();
                channel.vibrato.update();
                break;
            }
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
#ifdef FMUSIC_XM_NOTEDELAY_ACTIVE
                    case XMSpecialEffect::NOTEDELAY:
                    {
                        if (tick_ == paramy)
                        {
                            //= PROCESS INSTRUMENT NUMBER ==================================================================
                            const XMSampleHeader& sample_header = module_->getInstrument(channel.inst).getSample(channel.note).header;
                            channel.reset(sample_header.default_volume, sample_header.default_panning);
                            channel.period = channel.period_target;
                            channel.processVolumeByteNote(volume);
                            channel.trigger = true;
                        }
                        break;
                    }
#endif
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
                }
                break;
            }
#ifdef FMUSIC_XM_GLOBALVOLSLIDE_ACTIVE
            case XMEffect::GLOBALVOLSLIDE:
            {
                global_volume_ += global_volume_slide_;
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
    }
}

PlayerState::PlayerState(std::unique_ptr<Module> module, unsigned int mix_rate) :
    module_{ std::move(module) },
    mixer_{ [this] {return this->tick(); }, module_->header_.default_bpm, mix_rate },
    global_volume_{ 64 },
    tick_{ 0 },
    ticks_per_row_{ module_->header_.default_tempo },
    pattern_delay_{ 0 },
    current_{ 0, 0 },
    next_{ 0, 0 }
{
    for (int channel_index = 0; channel_index < static_cast<int>(module_->header_.channels_count); channel_index++)
    {
        channels_[channel_index].index = channel_index;
    }
}
