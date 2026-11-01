#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Frame.h"

namespace telemetry
{
namespace protocol
{
enum class EncodeStatus : std::uint8_t
{
    kOk = 0U,
    kInvalidVersion,
    kUnknownMessageType,
    kPayloadTooLarge
};

class ProtocolEncoder final
{
public:
    static EncodeStatus encode(
        const Frame& frame,
        std::array<std::uint8_t, kMaxFrameSize>& output,
        std::size_t& output_size) noexcept;
};

} // namespace protocol
} // namespace telemetry
