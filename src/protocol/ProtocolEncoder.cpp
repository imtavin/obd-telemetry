#include "ProtocolEncoder.h"

#include "Crc16.h"

namespace telemetry
{
namespace protocol
{
EncodeStatus ProtocolEncoder::encode(
    const Frame& frame,
    std::array<std::uint8_t, kMaxFrameSize>& output,
    std::size_t& output_size) noexcept
{
    output.fill(0U);
    output_size = 0U;

    if (frame.version != kProtocolVersion)
    {
        return EncodeStatus::kInvalidVersion;
    }

    if (!is_known_message_type(frame.type))
    {
        return EncodeStatus::kUnknownMessageType;
    }

    if (frame.payload_length > kMaxPayloadSize)
    {
        return EncodeStatus::kPayloadTooLarge;
    }

    output[0U] = frame.version;
    output[1U] = static_cast<std::uint8_t>(frame.type);
    output[2U] = static_cast<std::uint8_t>(frame.sequence & 0x00FFU);
    output[3U] = static_cast<std::uint8_t>((frame.sequence >> 8U) & 0x00FFU);
    output[4U] = frame.payload_length;

    for (std::size_t index = 0U; index < frame.payload_length; ++index)
    {
        output[kFrameHeaderSize + index] = frame.payload[index];
    }

    const std::size_t crc_offset = kFrameHeaderSize + frame.payload_length;
    const std::uint16_t crc = Crc16::calculate(output.data(), crc_offset);
    output[crc_offset] = static_cast<std::uint8_t>(crc & 0x00FFU);
    output[crc_offset + 1U] = static_cast<std::uint8_t>((crc >> 8U) & 0x00FFU);
    output_size = crc_offset + kCrcSize;

    return EncodeStatus::kOk;
}

} // namespace protocol
} // namespace telemetry
