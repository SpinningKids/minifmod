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

#pragma once

#include <cassert>

#include <xmformat/pattern_cell.h>

// pattern data type
class Pattern
{
	int size_{ 64 };
	XMPatternCell data_[256][32]{}; // uninitialized on purpose
public:
	Pattern() noexcept = default;

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
