
#pragma once

#include <cassert>

#include <xmformat/pattern_cell.h>

// pattern data type
class Pattern
{
	int size_;
	XMPatternCell data_[256][32]; // uninitialized on purpose
public:
	Pattern() noexcept : size_{ 64 } {}

	[[nodiscard]] int size() const noexcept { return size_; }
	void resize(int size)
	{
		assert(size >= 0 && size <= 256);
		size_ = size;
	}

	[[nodiscard]] auto operator[](int row) const -> decltype(data_[row])
	{
		assert(row >= 0 && row < size_);
		return data_[row];
	}

	[[nodiscard]] auto operator[](int row) -> decltype(data_[row])
	{
		assert(row >= 0 && row < size_);
		return data_[row];
	}
};
