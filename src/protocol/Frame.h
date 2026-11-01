#pragma once

#include <array>
#include <cstdint>

#include "ProtocolTypes.h"

namespace telemetry
{
namespace protocol
{
struct Frame
{
    std::uint8_t version = kProtocolVersion;
    MessageType type = MessageType::kHello;
    std::uint16_t sequence = 0U;
    std::uint8_t payload_length = 0U;
    std::array<std::uint8_t, kMaxPayloadSize> payload{};
};

} // namespace protocol
} // namespace telemetry
