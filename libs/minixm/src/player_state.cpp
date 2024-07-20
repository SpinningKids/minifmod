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

#include <minixm/player_state.h>
#include <minixm/xmeffects.h>

#include <xmformat/sample_header.h>

namespace
{
    int GetXMLinearPeriodFinetuned(XMNote note, int8_t fine_tune) noexcept
    {
        return 121 * 128 - note.value * 128 - fine_tune;
    }

#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
    int GetAmigaPeriodFinetuned(XMNote note, int8_t fine_tune) noexcept
    {
        if (fine_tune < 0) return GetAmigaPeriodFinetuned(note - 1, static_cast<int8_t>(128 - fine_tune));

        static constexpr int16_t amiga_periods[13] = {
            27392, 25855, 24403, 23034, 21741, 20521, 19369, 18282, 17256, 16287, 15373, 14510, 13696
        };

        const uint8_t tone = note.value % 12;
        const uint8_t octave = note.value / 12;

        const int base_period = amiga_periods[tone];

        // interpolate for finer tuning
        return (base_period + (((amiga_periods[tone + 1] - base_period) * fine_tune) >> 7)) >> octave;
    }
#endif

    int GetPeriodFinetuned(XMNote note, int8_t fine_tune, bool linear) noexcept
    {
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
        if (!linear)
            return GetAmigaPeriodFinetuned(note, fine_tune);
#endif
        return GetXMLinearPeriodFinetuned(note, fine_tune);
    }

    int GetPeriodDeltaFinetuned(XMNote note, int8_t delta, int8_t fine_tune, bool linear) noexcept
    {
#ifdef FMUSIC_XM_AMIGAPERIODS_ACTIVE
        if (!linear)
            return GetAmigaPeriodFinetuned(note + delta, fine_tune) - GetAmigaPeriodFinetuned(note, fine_tune);
#endif
        return delta * 128;
    }
}

Position PlayerState::tick()
{
    if (tick_ == 0) // new note
    {
        updateNote(); // Update and play the note
    }
    else
    {
        updateTick(); // Else update the inbetween row effects
    }

    clampGlobalVolume();
    for (int channel_index = 0; channel_index < module_->header_.channels_count; channel_index++)
    {
        Channel& channel = channels_[channel_index];
        channel.updateVolume();
        Instrument& instrument = module_->getInstrument(channel.instrument_index);
        channel.processInstrument(instrument);
        channel.sendToMixer(mixer_, instrument, global_volume_,
                            module_->header_.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY);
    }

    tick_++;
    if (tick_ >= ticks_per_row_ + pattern_delay_)
    {
        pattern_delay_ = 0;
        tick_ = 0;
    }
    return current_;
}

void PlayerState::updateNote()
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
        const auto& [note, instrument_number, volume, effect, effect_parameter] = row[channel_index];
        Channel& channel = channels_[channel_index];

        const int paramx = effect_parameter >> 4; // get effect param x
        const int paramy = effect_parameter & 0xF; // get effect param y
        const int slide = paramx ? paramx : -paramy;

        const bool porta = effect == XMEffect::PORTATO || effect == XMEffect::PORTATO_VOLUME_SLIDE;
        const bool valid_note = note.isValid();

        if (!porta)
        {
            // first store note and instrument number if there was one
            if (instrument_number) //  bugfix 3.20 (&& !porta)
            {
                channel.instrument_index = instrument_number - 1; // remember the Instrument #
            }

            if (valid_note) //  bugfix 3.20 (&& !porta)
            {
                channel.note = note; // remember the note
            }
        }

        Instrument& instrument = module_->getInstrument(channel.instrument_index);
        const XMSampleHeader& sample_header = instrument.getSample(channel.note).header;

        const int old_volume = channel.volume;
        const int old_period = channel.period;
        const int old_pan = channel.pan;

        // if there is no more tremolo, set volume to volume + last tremolo delta
        if (channel.last_effect == XMEffect::TREMOLO && effect != XMEffect::TREMOLO)
        {
            channel.volume += channel.volume_delta;
        }
        channel.last_effect = effect;

        channel.volume_delta = 0;
        channel.trigger = valid_note;
        channel.period_delta = 0; // this is for vibrato / arpeggio etc
        channel.stop = false;

        //= PROCESS NOTE ===============================================================================
        if (valid_note)
        {
            channel.fine_tune = sample_header.fine_tune;

            // get note according to relative note
            channel.real_note = note + sample_header.relative_note;

            // get period according to realnote and fine_tune
            channel.period_target = GetPeriodFinetuned(channel.real_note, channel.fine_tune,
                                                       module_->header_.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY);

            // frequency only changes if there are no portamento effects
            if (!porta)
            {
                channel.period = channel.period_target;
            }
        }

        //= PROCESS INSTRUMENT NUMBER ==================================================================
        if (instrument_number)
        {
            channel.reset(sample_header.default_volume, sample_header.default_panning);
            instrument.reset();
        }

        //= PROCESS VOLUME BYTE ========================================================================
        channel.processVolumeByteNote(volume);

        //= PROCESS KEY OFF ============================================================================
        if (note.isKeyOff() || effect == XMEffect::KEY_OFF)
        {
            channel.key_off = true;
        }

#ifdef FMUSIC_XM_VOLUMEENVELOPE_ACTIVE
        if (instrument.volume_envelope.count == 0 && channel.key_off)
        {
            channel.volume_envelope.reset(0.0f);
        }
#endif

        //= PROCESS TICK 0 EFFECTS =====================================================================
        switch (effect)
        {
#ifdef FMUSIC_XM_PORTAUP_ACTIVE
        case XMEffect::PORTA_UP:
            {
                if (effect_parameter)
                {
                    channel.porta_up = effect_parameter;
                }
                break;
            }
#endif
#ifdef FMUSIC_XM_PORTADOWN_ACTIVE
        case XMEffect::PORTA_DOWN:
            {
                if (effect_parameter)
                {
                    channel.porta_down = effect_parameter;
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
        case XMEffect::PORTATO_VOLUME_SLIDE:
            {
                channel.portamento.setTarget(channel.period_target);
                channel.setVolSlide(slide);
                channel.trigger = false;
                break;
            }
#endif
#ifdef FMUSIC_XM_VIBRATOVOLSLIDE_ACTIVE
        case XMEffect::VIBRATO_VOLUME_SLIDE:
            {
                channel.setVolSlide(slide);
                channel.period_delta = channel.vibrato();
                break; // not processed on tick 0
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
        case XMEffect::SET_PAN_POSITION:
            {
                channel.pan = effect_parameter;
                break;
            }
#endif
#ifdef FMUSIC_XM_SETSAMPLEOFFSET_ACTIVE
        case XMEffect::SET_SAMPLE_OFFSET:
            {
                const auto sample_offset = static_cast<uint32_t>(effect_parameter) * 256;
                if (sample_offset < sample_header.loop_start + sample_header.loop_length)
                {
                    mixer_.getChannel(channel.index).sample_offset = sample_offset;
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
        case XMEffect::VOLUME_SLIDE:
            {
                channel.setVolSlide(slide);
                break;
            }
#endif
#ifdef FMUSIC_XM_PATTERNJUMP_ACTIVE
        case XMEffect::PATTERN_JUMP: // --- 00 B00 : --- 00 D63 , should put us at ord=0, row=63
            {
                next_.order = effect_parameter % module_->header_.song_length;
                next_.row = 0;
                row_set = true;
                break;
            }
#endif
#ifdef FMUSIC_XM_SETVOLUME_ACTIVE
        case XMEffect::SET_VOLUME:
            {
                channel.volume = effect_parameter;
                break;
            }
#endif
#ifdef FMUSIC_XM_PATTERNBREAK_ACTIVE
        case XMEffect::PATTERN_BREAK:
            {
                next_.row = paramx * 10 + paramy;
                if (next_.row > 63) // NOTE: This seems odd, as the pattern might be longer than 64
                {
                    next_.row = 0;
                }
                if (!row_set)
                {
                    next_.order = (current_.order + 1) % module_->header_.song_length;
                    // NOTE: shouldn't we go to the restart_position?
                }
                row_set = true;
                break;
            }
#endif
        // extended PT effects
        case XMEffect::SPECIAL:
            {
                switch (static_cast<XMSpecialEffect>(paramx))
                {
#ifdef FMUSIC_XM_FINEPORTAUP_ACTIVE
                case XMSpecialEffect::FINE_PORTA_UP:
                    {
                        if (paramy)
                        {
                            channel.fine_porta_up = paramy;
                        }
                        channel.period -= channel.fine_porta_up * 8;
                        break;
                    }
#endif
#ifdef FMUSIC_XM_FINEPORTADOWN_ACTIVE
                case XMSpecialEffect::FINE_PORTA_DOWN:
                    {
                        if (paramy)
                        {
                            channel.fine_porta_down = paramy;
                        }
                        channel.period += channel.fine_porta_down * 8;
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
                case XMSpecialEffect::SET_VIBRATO_WAVE:
                    {
                        channel.vibrato.setFlags(paramy);
                        break;
                    }
#endif
#ifdef FMUSIC_XM_SETFINETUNE_ACTIVE
                case XMSpecialEffect::SET_FINE_TUNE:
                    {
                        channel.fine_tune = paramy;
                        break;
                    }
#endif
#ifdef FMUSIC_XM_PATTERNLOOP_ACTIVE
                case XMSpecialEffect::PATTERN_LOOP:
                    {
                        if (paramy == 0)
                        {
                            channel.pattern_loop_row = current_.row;
                        }
                        else
                        {
                            if (channel.pattern_loop_count == 0)
                            {
                                channel.pattern_loop_count = paramy;
                            }
                            else
                            {
                                channel.pattern_loop_count--;
                            }
                            if (channel.pattern_loop_count)
                            {
                                next_.row = channel.pattern_loop_row;
                                //nextorder = order; // This is not needed, as we initially set order = nextorder;
                                row_set = true;
                            }
                        }
                        break;
                    }
#endif
#if defined(FMUSIC_XM_TREMOLO_ACTIVE) && defined(FMUSIC_XM_SETTREMOLOWAVE_ACTIVE)
                case XMSpecialEffect::SET_TREMOLO_WAVE:
                    {
                        channel.tremolo.setFlags(paramy);
                        break;
                    }
#endif
#ifdef FMUSIC_XM_SETPANPOSITION16_ACTIVE
                case XMSpecialEffect::SET_PAN_POSITION_16:
                    {
                        channel.pan = paramy * 16;
                        break;
                    }
#endif
#ifdef FMUSIC_XM_FINEVOLUMESLIDEUP_ACTIVE
                case XMSpecialEffect::FINE_VOLUME_SLIDE_UP:
                    {
                        if (paramy)
                        {
                            channel.fine_volume_slide_up = paramy;
                        }
                        channel.volume += channel.fine_volume_slide_up;
                        break;
                    }
#endif
#ifdef FMUSIC_XM_FINEVOLUMESLIDEDOWN_ACTIVE
                case XMSpecialEffect::FINE_VOLUME_SLIDE_DOWN:
                    {
                        if (paramy)
                        {
                            channel.fine_volume_slide_down = paramy;
                        }
                        channel.volume -= channel.fine_volume_slide_down;
                        break;
                    }
#endif
#ifdef FMUSIC_XM_NOTEDELAY_ACTIVE
                case XMSpecialEffect::NOTE_DELAY:
                    {
                        channel.volume = old_volume;
                        channel.period = old_period;
                        channel.pan = old_pan;
                        channel.trigger = false;
                        break;
                    }
#endif
#ifdef FMUSIC_XM_PATTERNDELAY_ACTIVE
                case XMSpecialEffect::PATTERN_DELAY:
                    {
                        pattern_delay_ = ticks_per_row_ * paramy;
                        break;
                    }
#endif
                }
                break;
            }
#ifdef FMUSIC_XM_SETSPEED_ACTIVE
        case XMEffect::SET_SPEED:
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
        case XMEffect::SET_GLOBAL_VOLUME:
            {
                global_volume_ = effect_parameter;
                break;
            }
#endif
#ifdef FMUSIC_XM_GLOBALVOLSLIDE_ACTIVE
        case XMEffect::GLOBAL_VOLUME_SLIDE:
            {
                if (slide)
                {
                    global_volume_slide_ = slide;
                }
                break;
            }
#endif
#if defined(FMUSIC_XM_SETENVELOPEPOS_ACTIVE) && defined(FMUSIC_XM_VOLUMEENVELOPE_ACTIVE)
        case XMEffect::SET_ENVELOPE_POSITION:
            {
                channel.volume_envelope.setPosition(effect_parameter);
                break;
            }
#endif
#ifdef FMUSIC_XM_PANSLIDE_ACTIVE
        case XMEffect::PAN_SLIDE:
            {
                channel.setPanSlide(slide);
                break;
            }
#endif
#ifdef FMUSIC_XM_MULTIRETRIG_ACTIVE
        case XMEffect::MULTI_RETRIGGER:
            {
                if (effect_parameter)
                {
                    channel.retrigger_volume_operator = static_cast<XMRetriggerVolumeOperation>(paramx);
                    channel.retrigger_tick = paramy;
                }
                break;
            }
#endif
#ifdef FMUSIC_XM_TREMOR_ACTIVE
        case XMEffect::TREMOR:
            {
                if (effect_parameter)
                {
                    channel.tremor_on = paramx + 1;
                    channel.tremor_off = paramy + 1;
                }
                channel.tremor();
                break;
            }
#endif
#ifdef FMUSIC_XM_EXTRAFINEPORTA_ACTIVE
        case XMEffect::EXTRA_FINE_PORTA:
            {
                switch (paramx)
                {
                case 1:
                    {
                        if (paramy)
                        {
                            channel.extra_fine_porta_up = paramy;
                        }
                        channel.period -= channel.extra_fine_porta_up * 2;
                        break;
                    }
                case 2:
                    {
                        if (paramy)
                        {
                            channel.extra_fine_porta_down = paramy;
                        }
                        channel.period += channel.extra_fine_porta_down * 2;
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
        if (next_.row >= pattern.size()) // if end of pattern
        {
            next_.row = 0; // start at top of pattn
            next_.order = current_.order + 1;
            if (next_.order >= module_->header_.song_length)
            {
                next_.order = module_->header_.restart_position;
            }
        }
    }
}

void PlayerState::updateTick()
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

        const int paramx = effect_parameter >> 4; // get effect param x
        const int paramy = effect_parameter & 0xF; // get effect param y

        channel.volume_delta = 0;
        channel.trigger = false;
        channel.period_delta = 0; // this is for vibrato / arpeggio etc
        channel.stop = false;

        //= PROCESS VOLUME BYTE ========================================================================
        channel.processVolumeByteTick(volume);

        //= PROCESS TICK N != 0 EFFECTS =====================================================================
        switch (effect)
        {
#ifdef FMUSIC_XM_ARPEGGIO_ACTIVE
        case XMEffect::ARPEGGIO:
            {
                int8_t v = 0;
                switch (tick_ % 3)
                {
                case 1:
                    v = paramx;
                    break;
                case 2:
                    v = paramy;
                    break;
                }
                channel.period_delta = GetPeriodDeltaFinetuned(channel.real_note, v, channel.fine_tune,
                                                               module_->header_.flags & FMUSIC_XMFLAGS_LINEARFREQUENCY);
                break;
            }
#endif
#ifdef FMUSIC_XM_PORTAUP_ACTIVE
        case XMEffect::PORTA_UP:
            {
                channel.period_delta = 0;
                channel.period = std::max(channel.period - (channel.porta_up * 8), 112);
                // subtract period and stop at B#8
                break;
            }
#endif
#ifdef FMUSIC_XM_PORTADOWN_ACTIVE
        case XMEffect::PORTA_DOWN:
            {
                channel.period_delta = 0;
                channel.period += channel.porta_down * 8; // subtract period
                break;
            }
#endif
#ifdef FMUSIC_XM_PORTATOVOLSLIDE_ACTIVE
        case XMEffect::PORTATO_VOLUME_SLIDE:
            {
                channel.volume += channel.volume_slide;
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
        case XMEffect::VIBRATO_VOLUME_SLIDE:
            {
                channel.volume += channel.volume_slide;
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
                channel.volume_delta = channel.tremolo();
                channel.tremolo.update();
                break;
            }
#endif
#ifdef FMUSIC_XM_VOLUMESLIDE_ACTIVE
        case XMEffect::VOLUME_SLIDE:
            {
                channel.volume += channel.volume_slide;
                break;
            }
#endif
        // extended PT effects
        case XMEffect::SPECIAL:
            {
                switch (static_cast<XMSpecialEffect>(paramx))
                {
#ifdef FMUSIC_XM_NOTEDELAY_ACTIVE
                case XMSpecialEffect::NOTE_DELAY:
                    {
                        if (tick_ == paramy)
                        {
                            //= PROCESS INSTRUMENT NUMBER ==================================================================
                            const XMSampleHeader& sample_header = module_->getInstrument(channel.instrument_index).
                                                                           getSample(channel.note).header;
                            channel.reset(sample_header.default_volume, sample_header.default_panning);
                            channel.period = channel.period_target;
                            channel.processVolumeByteNote(volume);
                            channel.trigger = true;
                        }
                        break;
                    }
#endif
#ifdef FMUSIC_XM_RETRIG_ACTIVE
                case XMSpecialEffect::RETRIGGER:
                    {
                        if (paramy && tick_ % paramy == 0)
                        {
                            channel.trigger = true;
                        }
                        break;
                    }
#endif
#ifdef FMUSIC_XM_NOTECUT_ACTIVE
                case XMSpecialEffect::NOTE_CUT:
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
        case XMEffect::GLOBAL_VOLUME_SLIDE:
            {
                global_volume_ += global_volume_slide_;
                break;
            }
#endif
#ifdef FMUSIC_XM_PANSLIDE_ACTIVE
        case XMEffect::PAN_SLIDE:
            {
                channel.pan += channel.pan_slide;
                break;
            }
#endif
#ifdef FMUSIC_XM_MULTIRETRIG_ACTIVE
        case XMEffect::MULTI_RETRIGGER:
            {
                if (channel.retrigger_tick && !(tick_ % channel.retrigger_tick))
                {
                    switch (channel.retrigger_volume_operator)
                    {
                    case XMRetriggerVolumeOperation::NONE:
                        {
                            break;
                        }
                    case XMRetriggerVolumeOperation::DECREASE_1:
                        {
                            channel.volume--;
                            break;
                        }
                    case XMRetriggerVolumeOperation::DECREASE_2:
                        {
                            channel.volume -= 2;
                            break;
                        }
                    case XMRetriggerVolumeOperation::DECREASE_4:
                        {
                            channel.volume -= 4;
                            break;
                        }
                    case XMRetriggerVolumeOperation::DECREASE_8:
                        {
                            channel.volume -= 8;
                            break;
                        }
                    case XMRetriggerVolumeOperation::DECREASE_16:
                        {
                            channel.volume -= 16;
                            break;
                        }
                    case XMRetriggerVolumeOperation::SUBTRACT_33_PERCENT:
                        {
                            channel.volume = channel.volume * 2 / 3;
                            break;
                        }
                    case XMRetriggerVolumeOperation::HALVE:
                        {
                            channel.volume /= 2;
                            break;
                        }
                    case XMRetriggerVolumeOperation::INCREASE_1:
                        {
                            channel.volume++;
                            break;
                        }
                    case XMRetriggerVolumeOperation::INCREASE_2:
                        {
                            channel.volume += 2;
                            break;
                        }
                    case XMRetriggerVolumeOperation::INCREASE_4:
                        {
                            channel.volume += 4;
                            break;
                        }
                    case XMRetriggerVolumeOperation::INCREASE_8:
                        {
                            channel.volume += 8;
                            break;
                        }
                    case XMRetriggerVolumeOperation::INCREASE_16:
                        {
                            channel.volume += 16;
                            break;
                        }
                    case XMRetriggerVolumeOperation::ADD_50_PERCENT:
                        {
                            channel.volume = channel.volume * 3 / 2;
                            break;
                        }
                    case XMRetriggerVolumeOperation::DOUBLE:
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
    module_{std::move(module)},
    mixer_{[](void* context) { return static_cast<PlayerState*>(context)->tick(); }, this, module_->header_.default_bpm, mix_rate},
    global_volume_{64},
    tick_{0},
    ticks_per_row_{module_->header_.default_tempo},
    pattern_delay_{0},
    current_{0, 0},
    next_{0, 0}
{
    for (int channel_index = 0; channel_index < static_cast<int>(module_->header_.channels_count); channel_index++)
    {
        channels_[channel_index].index = channel_index;
    }
}
