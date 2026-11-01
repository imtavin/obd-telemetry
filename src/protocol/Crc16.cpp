#include "Crc16.h"

namespace telemetry
{
namespace protocol
{
std::uint16_t Crc16::calculate(const std::uint8_t* data, const std::size_t length) noexcept
{
    std::uint16_t crc = 0xFFFFU;

    for (std::size_t index = 0U; index < length; ++index)
    {
        crc ^= static_cast<std::uint16_t>(data[index]) << 8U;

        for (std::uint8_t bit = 0U; bit < 8U; ++bit)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = static_cast<std::uint16_t>((crc << 1U) ^ 0x1021U);
            }
            else
            {
                crc = static_cast<std::uint16_t>(crc << 1U);
            }
        }
    }

    return crc;
}

} // namespace protocol
} // namespace telemetry
