
#pragma once

#include <cstdint>

#pragma pack(push)
#pragma pack(1)

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

#pragma pack(pop)
