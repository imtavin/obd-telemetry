#pragma once

#include <cstddef>
#include <cstdint>

namespace telemetry
{
namespace protocol
{
class Crc16 final
{
public:
    static std::uint16_t calculate(const std::uint8_t* data, std::size_t length) noexcept;
};

} // namespace protocol
} // namespace telemetry
