
#pragma once

#include <cstdint>

#pragma pack(push)
#pragma pack(1)

struct XMHeader
{
	char		header[17];
	char		module_name[20];
	char		id; // 0x01a
	char		tracker_name[20];
	uint16_t	tracker_version;
	uint32_t	header_size;
	uint16_t	song_length;
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

struct XMNote
{
	unsigned char	note;			// note to play at     (0-133) 132=none,133=keyoff
	unsigned char	number;			// sample being played (0-99)
	unsigned char	volume;			// volume column value (0-64)  255=no volume
	unsigned char	effect;			// effect number       (0-1Ah)
	unsigned char	eparam;			// effect parameter    (0-255)
};

static_assert(sizeof(XMNote) == 5);

struct XMInstrumentHeader
{
	uint32_t	instSize;               // instrument size
	char		instName[22];           // instrument filename
	uint8_t		instType;               // instrument type (now 0)
	uint16_t	numSamples;             // number of samples in instrument
};

static_assert(sizeof(XMInstrumentHeader) == 29);

struct XMInstrumentSampleHeader
{
	uint32_t	headerSize;             // sample header size
	uint8_t		noteSmpNums[96];        // sample numbers for notes
	uint16_t	volEnvelope[24];        // volume envelope points
	uint16_t	panEnvelope[24];        // panning envelope points
	uint8_t		numVolPoints;           // number of volume envelope points
	uint8_t		numPanPoints;           // number of panning env. points
	uint8_t		volSustain;             // volume sustain point
	uint8_t		volLoopStart;           // volume loop start point
	uint8_t		volLoopEnd;             // volume loop end point
	uint8_t		panSustain;             // panning sustain point
	uint8_t		panLoopStart;           // panning loop start point
	uint8_t		panLoopEnd;             // panning loop end point
	uint8_t		volEnvFlags;            // volume envelope flags
	uint8_t		panEnvFlags;            // panning envelope flags
	uint8_t		vibType;                // vibrato type
	uint8_t		vibSweep;               // vibrato sweep
	uint8_t		vibDepth;               // vibrato depth
	uint8_t		vibRate;                // vibrato rate
	uint16_t	volFadeout;             // volume fadeout
	uint16_t	reserved;
};

static_assert(sizeof(XMInstrumentSampleHeader) == 214);

struct XMSampleHeader
{
	uint32_t	length;
	uint32_t	loop_start;
	uint32_t	loop_length;
	uint8_t		default_volume;
	int8_t		finetune;
	uint8_t		loop_mode : 2;
	uint8_t		skip_bits_1 : 2;
	uint8_t		bits16 : 1;
	uint8_t		default_panning;
	int8_t		relative_note;
	uint8_t		reserved;
	char		sample_name[22];
};

static_assert(sizeof(XMSampleHeader) == 40);

#pragma pack(pop)
