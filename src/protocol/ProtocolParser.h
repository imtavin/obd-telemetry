#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Frame.h"

namespace telemetry
{
namespace protocol
{
enum class ParseStatus : std::uint8_t
{
    kInProgress = 0U,
    kFrameReady,
    kInvalidVersion,
    kUnknownMessageType,
    kInvalidLength,
    kInvalidCrc,
    kTimeout
};

class ProtocolParser final
{
public:
    ProtocolParser() noexcept;

    void reset() noexcept;

    // now_ms must come from a monotonic clock. Keeping the timestamp outside
    // the parser avoids coupling this protocol component to FreeRTOS.
    ParseStatus consume(std::uint8_t byte, Frame& frame, std::uint32_t now_ms) noexcept;

    // The owner of the BLE receive path calls this when a fragmented frame
    // exceeds its configured deadline. No frame is delivered on timeout.
    ParseStatus expire(std::uint32_t now_ms, std::uint32_t timeout_ms) noexcept;

    bool has_partial_frame() const noexcept;

private:
    std::array<std::uint8_t, kMaxFrameSize> buffer_{};
    std::size_t buffered_size_ = 0U;
    std::size_t expected_frame_size_ = 0U;
    std::uint32_t last_byte_timestamp_ms_ = 0U;
    bool has_timestamp_ = false;
};

} // namespace protocol
} // namespace telemetry
