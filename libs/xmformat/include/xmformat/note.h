
#pragma once

#include <cstdint>

#pragma pack(push)
#pragma pack(1)

struct XMNote
{
	static constexpr uint8_t KEY_OFF = 97;
	uint8_t value; // note to play at     (0-133) 132=none,133=keyoff <- This comment is WRONG.

	bool isKeyOff() const noexcept { return value == KEY_OFF; }
	bool isValid() const noexcept { return value && value != KEY_OFF; }

	XMNote operator + (int8_t rel) const noexcept
	{
		return { (uint8_t)(value + rel) };
	}

	XMNote operator - (int8_t rel) const noexcept
	{
		return { (uint8_t)(value - rel) };
	}

	void turnOff() noexcept { value = KEY_OFF; }
};

static_assert(sizeof(XMNote) == 1);

#pragma pack(pop)
