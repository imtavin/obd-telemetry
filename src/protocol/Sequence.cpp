#include "Sequence.h"

namespace telemetry
{
namespace protocol
{
void SequenceTracker::reset() noexcept
{
    last_sequence_ = 0U;
    initialized_ = false;
}

SequenceStatus SequenceTracker::observe(const std::uint16_t sequence) noexcept
{
    if (!initialized_)
    {
        last_sequence_ = sequence;
        initialized_ = true;
        return SequenceStatus::kFirst;
    }

    const std::uint16_t distance = static_cast<std::uint16_t>(sequence - last_sequence_);
    if (distance == 0U)
    {
        return SequenceStatus::kDuplicate;
    }

    if (distance == 1U)
    {
        last_sequence_ = sequence;
        return SequenceStatus::kExpected;
    }

    // A forward distance below half the sequence space is a newer frame with
    // one or more missing frames. Older/reordered frames are not committed as
    // the new baseline so a later valid frame can still be accepted.
    if (distance < 0x8000U)
    {
        last_sequence_ = sequence;
        return SequenceStatus::kGap;
    }

    return SequenceStatus::kOutOfOrder;
}

bool SequenceTracker::initialized() const noexcept
{
    return initialized_;
}

std::uint16_t SequenceTracker::last_sequence() const noexcept
{
    return last_sequence_;
}

SequenceGenerator::SequenceGenerator(const std::uint16_t first_sequence) noexcept
    : next_sequence_(first_sequence)
{
}

void SequenceGenerator::reset(const std::uint16_t first_sequence) noexcept
{
    next_sequence_ = first_sequence;
}

std::uint16_t SequenceGenerator::next() noexcept
{
    const std::uint16_t sequence = next_sequence_;
    next_sequence_ = static_cast<std::uint16_t>(next_sequence_ + 1U);
    return sequence;
}

} // namespace protocol
} // namespace telemetry
