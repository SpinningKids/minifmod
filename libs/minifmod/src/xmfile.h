
#pragma once

#include <bit>
#include <cstdint>

#pragma pack(push)
#pragma pack(1)

static_assert(std::endian::native == std::endian::little); // this will only work in little endian system

constexpr uint16_t FMUSIC_XMFLAGS_LINEARFREQUENCY = 1;

struct XMHeader
{
	char		header[17];
	char		module_name[20];
	char		id; // 0x01a
	char		tracker_name[20];
	uint16_t	tracker_version;
	uint32_t	header_size;
	uint16_t	song_length; // < 256
	uint16_t	restart_position;
	uint16_t	channels_count;
	uint16_t	patterns_count; // < 256
	uint16_t	instruments_count; // < 128
	uint16_t	flags; // 	0 - Linear frequency table / Amiga freq.table
	uint16_t	default_tempo;
	uint16_t	default_bpm;
	uint8_t		pattern_order[256];
};

static_assert(sizeof(XMHeader) == 60 + 20 + 256); // xm header is 336 bytes long

struct XMPatternHeader
{
	uint32_t	header_size;
	uint8_t		packing; // always 0
	uint16_t	rows; // must be < 256
	uint16_t	packed_pattern_data_size;
};

static_assert(sizeof(XMPatternHeader) == 9);

enum class XMEffect : uint8_t
{
	ARPEGGIO,
	PORTAUP,
	PORTADOWN,
	PORTATO,
	VIBRATO,
	PORTATOVOLSLIDE,
	VIBRATOVOLSLIDE,
	TREMOLO,
	SETPANPOSITION,
	SETSAMPLEOFFSET,
	VOLUMESLIDE,
	PATTERNJUMP,
	SETVOLUME,
	PATTERNBREAK,
	SPECIAL,
	SETSPEED,
	SETGLOBALVOLUME,
	GLOBALVOLSLIDE,
	I,
	J,
	KEYOFF,
	SETENVELOPEPOS,
	M,
	N,
	O,
	PANSLIDE,
	Q,
	MULTIRETRIG,
	S,
	TREMOR,
	U,
	V,
	W,
	EXTRAFINEPORTA,
	Y,
	Z,
};

enum class XMSpecialEffect
{
	SETFILTER,
	FINEPORTAUP,
	FINEPORTADOWN,
	SETGLISSANDO,
	SETVIBRATOWAVE,
	SETFINETUNE,
	PATTERNLOOP,
	SETTREMOLOWAVE,
	SETPANPOSITION16,
	RETRIG,
	FINEVOLUMESLIDEUP,
	FINEVOLUMESLIDEDOWN,
	NOTECUT,
	NOTEDELAY,
	PATTERNDELAY,
	FUNKREPEAT,
};

struct XMNote
{
	static constexpr uint8_t KEYOFF = 97;
	uint8_t value; // note to play at     (0-133) 132=none,133=keyoff <- This comment is WRONG.

	bool isKeyOff() const { return value == KEYOFF; }
	bool isValid() const { return value && value != KEYOFF; }

	XMNote operator + (int8_t rel) const
	{
		return { (uint8_t)(value + rel) };
	}

	void turnOff() { value = KEYOFF; }
};

static_assert(sizeof(XMNote) == 1);

struct XMPatternCell
{
	XMNote note;				
	uint8_t sample_index;		// sample being played (0-99)
	uint8_t volume;				// volume column value (0-64)  255=no volume
	XMEffect effect;			// effect number       (0-1Ah)
	uint8_t effect_parameter;	// effect parameter    (0-255)
};

static_assert(sizeof(XMPatternCell) == 5);

struct XMInstrumentHeader
{
	uint32_t	header_size;			// instrument size
	char		instrument_name[22];	// instrument filename
	uint8_t		instrument_type;		// instrument type (now 0)
	uint16_t	samples_count;			// number of samples in instrument
};

static_assert(sizeof(XMInstrumentHeader) == 29);

enum XMEnvelopeFlags : uint8_t
{
    XMEnvelopeFlagsOff = 0,
    XMEnvelopeFlagsOn = 1,
    XMEnvelopeFlagsSustain = 2,
    XMEnvelopeFlagsLoop = 4
};

enum class XMInstrumentVibratoType : uint8_t
{
    Sine = 0,
	Square = 1,
	InverseSawTooth = 2,
	SawTooth = 3
};

struct XMEnvelopePoint
{
	uint16_t position;
	uint16_t value;
};

struct XMInstrumentSampleHeader
{
	uint32_t				header_size;				// sample header size
	uint8_t					note_sample_number[96];		// sample numbers for notes
	XMEnvelopePoint			volume_envelope[12];		// volume envelope points
	XMEnvelopePoint			pan_envelope[12];			// panning envelope points
	uint8_t					volume_envelope_count;		// number of volume envelope points
	uint8_t					pan_envelope_count;			// number of panning env. points
	uint8_t					volume_sustain_index;		// volume sustain point
	uint8_t					volume_loop_start_index;	// volume loop start point
	uint8_t					volume_loop_end_index;		// volume loop end point
	uint8_t					pan_sustain_index;			// panning sustain point
	uint8_t					pan_loop_start_index;		// panning loop start point
	uint8_t					pan_loop_end_index;			// panning loop end point
	XMEnvelopeFlags			volume_envelope_flags;		// volume envelope flags
	XMEnvelopeFlags			pan_envelope_flags;			// panning envelope flags
	XMInstrumentVibratoType	vibrato_type;				// vibrato type
	uint8_t					vibrato_sweep;				// vibrato sweep
	uint8_t					vibrato_depth;				// vibrato depth
	uint8_t					vibrato_rate;				// vibrato rate
	uint16_t				volume_fadeout;				// volume fadeout
	uint16_t				reserved;
};

static_assert(sizeof(XMInstrumentSampleHeader) == 214);

enum class XMLoopMode : uint8_t
{
	Off = 0,
	Normal = 1,		// For forward looping samples.
	Bidi = 2,		// For bidirectional looping samples.  (no effect if in hardware).
};

struct XMSampleHeader
{
	uint32_t	length;
	uint32_t	loop_start;
	uint32_t	loop_length;
	uint8_t		default_volume;
	int8_t		fine_tune;
	XMLoopMode	loop_mode : 2;
	uint8_t		skip_bits_1 : 2;
	bool		bits16 : 1;
	uint8_t		default_panning;
	int8_t		relative_note;
	uint8_t		reserved;
	char		sample_name[22];
};

static_assert(sizeof(XMSampleHeader) == 40);

#pragma pack(pop)
