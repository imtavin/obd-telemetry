#pragma once

#include <cstdint>

namespace telemetry
{
namespace protocol
{
enum class SequenceStatus : std::uint8_t
{
    kFirst = 0U,
    kExpected,
    kGap,
    kDuplicate,
    kOutOfOrder
};

// One instance is owned by one direction of one BLE session. The session
// manager must reset it on connect/disconnect or when a new session is accepted.
class SequenceTracker final
{
public:
    SequenceTracker() noexcept = default;

    void reset() noexcept;
    SequenceStatus observe(std::uint16_t sequence) noexcept;
    bool initialized() const noexcept;
    std::uint16_t last_sequence() const noexcept;

private:
    std::uint16_t last_sequence_ = 0U;
    bool initialized_ = false;
};

// Sequence generation is also per direction and per session. Overflow is
// intentional: uint16_t subtraction in SequenceTracker handles wrap-around.
class SequenceGenerator final
{
public:
    explicit SequenceGenerator(std::uint16_t first_sequence = 0U) noexcept;

    void reset(std::uint16_t first_sequence = 0U) noexcept;
    std::uint16_t next() noexcept;

private:
    std::uint16_t next_sequence_ = 0U;
};

} // namespace protocol
} // namespace telemetry
