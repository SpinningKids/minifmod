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

#include <minixm/module.h>
#include <minixm/channel.h>
#include <minixm/xmeffects.h>

#include <xmformat/pattern_header.h>

Module::Module(const minifmod::FileAccess& fileAccess, void* fp, const std::function<void(int16_t*, size_t, int, int)>& sample_load_callback)
{
    fileAccess.seek(fp, 0, SEEK_SET);
    fileAccess.read(&header_, sizeof(header_), fp);
#ifndef FMUSIC_XM_AMIGAPERIODS_ACTIVE
    header_.flags |= FMUSIC_XMFLAGS_LINEARFREQUENCY;
#endif

    // seek to patterndata
    fileAccess.seek(fp, static_cast<int>(60 + header_.header_size), SEEK_SET);

    // unpack and read patterns
    for (int pattern_index = 0; pattern_index < static_cast<int>(header_.patterns_count); pattern_index++)
    {
        XMPatternHeader pattern_header;
        fileAccess.read(&pattern_header, sizeof(pattern_header), fp);

        Pattern& pattern = pattern_[pattern_index];
        pattern.resize(pattern_header.rows);

        if (pattern_header.packed_pattern_data_size > 0)
        {
            for (int row = 0; row < static_cast<int>(pattern_header.rows); ++row)
            {
                auto& current_row = pattern[row];

                for (int channel_index = 0; channel_index < static_cast<int>(header_.channels_count); channel_index++)
                {
                    unsigned char dat;

                    XMPatternCell& pattern_cell = current_row[channel_index];

                    fileAccess.read(&dat, 1, fp);
                    if (dat & 0x80)
                    {
                        if (dat & 1)  fileAccess.read(&pattern_cell.note, 1, fp);
                        if (dat & 2)  fileAccess.read(&pattern_cell.instrument_number, 1, fp);
                        if (dat & 4)  fileAccess.read(&pattern_cell.volume, 1, fp);
                        if (dat & 8)  fileAccess.read(&pattern_cell.effect, 1, fp);
                        if (dat & 16) fileAccess.read(&pattern_cell.effect_parameter, 1, fp);
                    }
                    else
                    {
                        pattern_cell.note = XMNote{ dat };
                        fileAccess.read(reinterpret_cast<char*>(&pattern_cell) + 1, sizeof(XMPatternCell) - 1, fp);
                    }

                    if (pattern_cell.instrument_number > 0x80)
                    {
                        pattern_cell.instrument_number = 0;
                    }
                }
            }
        }
    }

    // load instrument information
    for (int instrument_index = 0; instrument_index < static_cast<int>(header_.instruments_count); ++instrument_index)
    {
        // point a pointer to that particular instrument
        Instrument& instrument = instrument_[instrument_index];

        int first_sample_offset = fileAccess.tell(fp);
        fileAccess.read(&instrument.header, sizeof(instrument.header), fp);				// instrument size
        first_sample_offset += static_cast<int>(instrument.header.header_size);

        assert(instrument.header.samples_count <= 16);

        if (instrument.header.samples_count > 0)
        {
            fileAccess.read(&instrument.instrument_sample_header, sizeof(instrument.instrument_sample_header), fp);

            auto adjust_envelope = [](int count, const XMEnvelopePoint(&original_points)[12], int offset, float scale, XMEnvelopeFlags flags)
                {
                    EnvelopePoints e;
                    e.count = (count < 2 || !(flags & XMEnvelopeFlagsOn)) ? 0 : count;
                    for (int i = 0; i < e.count; ++i)
                    {
                        e.envelope[i].position = original_points[i].position;
                        e.envelope[i].value = static_cast<float>(original_points[i].value - offset) / scale;
                        if (i > 0)
                        {
                            const int tickdiff = e.envelope[i].position - e.envelope[i - 1].position;

                            e.envelope[i - 1].delta = tickdiff ? (e.envelope[i].value - e.envelope[i - 1].value) / static_cast<float>(tickdiff) : 0.f;
                        }
                    }
                    if (e.count) {
                        e.envelope[e.count - 1].delta = 0.f;
                    }
                    return e;
                };
#ifdef FMUSIC_XM_VOLUMEENVELOPE_ACTIVE
            instrument.volume_envelope = adjust_envelope(instrument.instrument_sample_header.volume_envelope_count, instrument.instrument_sample_header.volume_envelope, 0, 64, instrument.instrument_sample_header.volume_envelope_flags);
#endif
#ifdef FMUSIC_XM_PANENVELOPE_ACTIVE
            instrument.pan_envelope = adjust_envelope(instrument.instrument_sample_header.pan_envelope_count, instrument.instrument_sample_header.pan_envelope, 32, 32, instrument.instrument_sample_header.pan_envelope_flags);
#endif


            // seek to first sample
            fileAccess.seek(fp, first_sample_offset, SEEK_SET);
            for (int sample_index = 0; sample_index < static_cast<int>(instrument.header.samples_count); sample_index++)
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
            for (int sample_index = 0; sample_index < static_cast<int>(instrument.header.samples_count); sample_index++)
            {
                //unsigned int	samplelenbytes = sample.header.length * sample.bits / 8;

                //= ALLOCATE MEMORY FOR THE SAMPLE BUFFER ==============================================

                if (Sample& sample = instrument.sample[sample_index]; sample.header.length)
                {
                    sample.buff.reset(new int16_t[sample.header.length + 8]);

                    if (sample_load_callback)
                    {
                        sample_load_callback(sample.buff.get(), sample.header.length, instrument_index, sample_index);
                        fileAccess.seek(fp, static_cast<int>(sample.header.length * (sample.header.bits16 ? 2 : 1)), SEEK_CUR);
                    }
                    else
                    {
                        if (sample.header.bits16)
                        {
                            fileAccess.read(sample.buff.get(), static_cast<int>(sample.header.length * sizeof(short)), fp);
                        }
                        else
                        {
                            auto buff = std::unique_ptr<int8_t[]>(new int8_t[sample.header.length + 8]);
                            fileAccess.read(buff.get(), static_cast<int>(sample.header.length), fp);
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
            instrument.instrument_sample_header = XMInstrumentSampleHeader{};
            fileAccess.seek(fp, first_sample_offset, SEEK_SET);
        }
    }
}
