
#pragma once

#include <cassert>

#include "xmfile.h"

// pattern data type
class Pattern
{
	size_t size_;
	XMNote data_[256][32];
public:
	Pattern() noexcept : size_{ 64 }, data_{} {}

	[[nodiscard]] size_t size() const noexcept { return size_; }
	void resize(size_t size)
	{
		assert(size <= 256);
		size_ = size;
	}

	[[nodiscard]] auto operator[](size_t row) const noexcept -> decltype(data_[row])
	{
		assert(row < size_);
		return data_[row];
	}

	[[nodiscard]] auto operator[](size_t row) noexcept -> decltype(data_[row])
	{
		assert(row < size_);
		return data_[row];
	}
};
