
#pragma once

#include <functional>
#include <memory>

class IPlaybackDriver
{
	size_t mix_rate_;			// mixing rate in hz.
	size_t block_size_;			// LATENCY ms worth of samples
	size_t total_blocks_;
    size_t buffer_size_;		// size of 1 'latency' ms buffer in bytes
protected:
	IPlaybackDriver(unsigned int mix_rate, unsigned int buffer_size_ms, unsigned int latency);
public:
	static std::unique_ptr<IPlaybackDriver> create(unsigned int mix_rate, unsigned int buffer_size_ms, unsigned int latency);

	IPlaybackDriver() = default;
	IPlaybackDriver(const IPlaybackDriver&) = delete;
	IPlaybackDriver(IPlaybackDriver&&) = delete;
	IPlaybackDriver& operator = (const IPlaybackDriver&) = delete;
	IPlaybackDriver& operator = (IPlaybackDriver&&) = delete;
	virtual ~IPlaybackDriver() = default;

	size_t blocks() const noexcept { return total_blocks_; }
	size_t block_size() const noexcept { return block_size_; }
	size_t mix_rate() const noexcept { return mix_rate_; }
	size_t buffer_size() const noexcept { return buffer_size_; }

	virtual void start(std::function<void(size_t block, short data[])>&& fill) = 0;
	virtual void stop() = 0;

	virtual size_t current_block_played() const noexcept = 0;
};
