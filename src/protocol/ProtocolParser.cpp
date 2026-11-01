#include "ProtocolParser.h"

#include <algorithm>

#include "Crc16.h"

namespace telemetry
{
namespace protocol
{
ProtocolParser::ProtocolParser() noexcept
{
    reset();
}

void ProtocolParser::reset() noexcept
{
    buffer_.fill(0U);
    buffered_size_ = 0U;
    expected_frame_size_ = 0U;
    last_byte_timestamp_ms_ = 0U;
    has_timestamp_ = false;
}

ParseStatus ProtocolParser::consume(
    const std::uint8_t byte,
    Frame& frame,
    const std::uint32_t now_ms) noexcept
{
    if (buffered_size_ >= buffer_.size())
    {
        reset();
        return ParseStatus::kInvalidLength;
    }

    buffer_[buffered_size_++] = byte;
    last_byte_timestamp_ms_ = now_ms;
    has_timestamp_ = true;

    if (buffered_size_ == 1U && byte != kProtocolVersion)
    {
        reset();
        return ParseStatus::kInvalidVersion;
    }

    if (buffered_size_ == 2U)
    {
        const auto type = static_cast<MessageType>(buffer_[1U]);
        if (!is_known_message_type(type))
        {
            reset();
            return ParseStatus::kUnknownMessageType;
        }
    }

    if (buffered_size_ == kFrameHeaderSize)
    {
        const std::size_t payload_size = buffer_[4U];
        if (payload_size > kMaxPayloadSize)
        {
            reset();
            return ParseStatus::kInvalidLength;
        }

        expected_frame_size_ = kFrameHeaderSize + payload_size + kCrcSize;
    }

    if (expected_frame_size_ == 0U || buffered_size_ < expected_frame_size_)
    {
        return ParseStatus::kInProgress;
    }

    const std::size_t crc_offset = expected_frame_size_ - kCrcSize;
    const std::uint16_t expected_crc = Crc16::calculate(buffer_.data(), crc_offset);
    const std::uint16_t received_crc = static_cast<std::uint16_t>(buffer_[crc_offset]) |
        static_cast<std::uint16_t>(buffer_[crc_offset + 1U]) << 8U;

    if (expected_crc != received_crc)
    {
        reset();
        return ParseStatus::kInvalidCrc;
    }

    frame.version = buffer_[0U];
    frame.type = static_cast<MessageType>(buffer_[1U]);
    frame.sequence = static_cast<std::uint16_t>(buffer_[2U]) |
        static_cast<std::uint16_t>(buffer_[3U]) << 8U;
    frame.payload_length = buffer_[4U];
    frame.payload.fill(0U);
    std::copy_n(buffer_.begin() + static_cast<std::ptrdiff_t>(kFrameHeaderSize),
                frame.payload_length,
                frame.payload.begin());

    reset();
    return ParseStatus::kFrameReady;
}

ParseStatus ProtocolParser::expire(
    const std::uint32_t now_ms,
    const std::uint32_t timeout_ms) noexcept
{
    if (!has_partial_frame() || !has_timestamp_ || timeout_ms == 0U)
    {
        return ParseStatus::kInProgress;
    }

    // Unsigned subtraction makes this comparison safe across uint32_t clock
    // wrap-around as long as timeout_ms is less than half the clock range.
    if (static_cast<std::uint32_t>(now_ms - last_byte_timestamp_ms_) < timeout_ms)
    {
        return ParseStatus::kInProgress;
    }

    reset();
    return ParseStatus::kTimeout;
}

bool ProtocolParser::has_partial_frame() const noexcept
{
    return buffered_size_ != 0U;
}

} // namespace protocol
} // namespace telemetry
