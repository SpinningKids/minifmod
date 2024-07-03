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

#include <functional>
#include <memory>

class IPlaybackDriver
{
	uint32_t mix_rate_;			// mixing rate in hz.
	uint32_t block_size_;		// LATENCY ms worth of samples
	uint32_t total_blocks_;
    uint32_t buffer_size_;		// size of 1 'latency' ms buffer in bytes
protected:
	IPlaybackDriver(unsigned int mix_rate, unsigned int buffer_size_ms, unsigned int latency);
public:
	static std::unique_ptr<IPlaybackDriver> create(uint32_t mix_rate, uint32_t buffer_size_ms, uint32_t latency);

	IPlaybackDriver() = default;
	IPlaybackDriver(const IPlaybackDriver&) = delete;
	IPlaybackDriver(IPlaybackDriver&&) = delete;
	IPlaybackDriver& operator = (const IPlaybackDriver&) = delete;
	IPlaybackDriver& operator = (IPlaybackDriver&&) = delete;
	virtual ~IPlaybackDriver() = default;

	[[nodiscard]] uint32_t blocks() const noexcept { return total_blocks_; }
	[[nodiscard]] uint32_t block_size() const noexcept { return block_size_; }
	[[nodiscard]] uint32_t mix_rate() const noexcept { return mix_rate_; }
	[[nodiscard]] uint32_t buffer_size() const noexcept { return buffer_size_; }

	virtual void start(std::function<void(size_t block, short data[])>&& fill) = 0;
	virtual void stop() = 0;

	[[nodiscard]] virtual size_t current_block_played() const = 0;
};
