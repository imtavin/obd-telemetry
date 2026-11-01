#include "SessionPolicy.h"

namespace telemetry
{
namespace control
{
void SessionPolicy::reset() noexcept
{
    state_ = SessionState::kDisconnected;
    authenticated_ = false;
    session_id_ = 0U;
    subscription_mask_ = 0U;
}

void SessionPolicy::on_ble_connected(const bool bonded_and_authenticated) noexcept
{
    // A new BLE connection always starts a new handshake. No state from a
    // previous peer is reused, even when the link is already authenticated.
    state_ = SessionState::kConnected;
    authenticated_ = bonded_and_authenticated;
    session_id_ = 0U;
    subscription_mask_ = 0U;
}

void SessionPolicy::on_ble_disconnected() noexcept
{
    reset();
}

CommandDecision SessionPolicy::accept_hello(
    const std::uint32_t session_id,
    const std::uint8_t minimum_version,
    const std::uint8_t maximum_version) noexcept
{
    if (!authenticated_)
    {
        return CommandDecision::kInvalidAuthentication;
    }

    if (state_ != SessionState::kConnected)
    {
        return CommandDecision::kInvalidState;
    }

    if (!protocol::is_compatible_version_range(minimum_version, maximum_version))
    {
        return CommandDecision::kInvalidPayload;
    }

    session_id_ = session_id;
    subscription_mask_ = 0U;
    state_ = SessionState::kSessionActive;
    return CommandDecision::kAccepted;
}

CommandDecision SessionPolicy::handle_command(
    const protocol::MessageType type,
    const std::uint16_t signal_mask) noexcept
{
    if (!authenticated_)
    {
        return CommandDecision::kInvalidAuthentication;
    }

    if (!has_session())
    {
        return CommandDecision::kInvalidState;
    }

    switch (type)
    {
    case protocol::MessageType::kGetCapabilities:
    case protocol::MessageType::kPing:
    case protocol::MessageType::kGetStatus:
        return CommandDecision::kAccepted;

    case protocol::MessageType::kSetSubscription:
        if (!protocol::is_valid_subscription_mask(signal_mask))
        {
            return CommandDecision::kInvalidPayload;
        }

        if (state_ == SessionState::kStreaming)
        {
            return CommandDecision::kInvalidState;
        }

        if (state_ == SessionState::kConfigured && subscription_mask_ == signal_mask)
        {
            return CommandDecision::kIdempotent;
        }

        subscription_mask_ = signal_mask;
        state_ = SessionState::kConfigured;
        return CommandDecision::kAccepted;

    case protocol::MessageType::kStart:
        if (state_ == SessionState::kStreaming)
        {
            return CommandDecision::kIdempotent;
        }

        if (state_ != SessionState::kConfigured)
        {
            return CommandDecision::kInvalidState;
        }

        state_ = SessionState::kStreaming;
        return CommandDecision::kAccepted;

    case protocol::MessageType::kStop:
        if (state_ == SessionState::kStreaming)
        {
            state_ = SessionState::kConfigured;
            return CommandDecision::kAccepted;
        }

        if (state_ == SessionState::kConfigured || state_ == SessionState::kSessionActive)
        {
            return CommandDecision::kIdempotent;
        }

        return CommandDecision::kInvalidState;

    case protocol::MessageType::kHello:
    case protocol::MessageType::kAck:
    case protocol::MessageType::kError:
    case protocol::MessageType::kPong:
    case protocol::MessageType::kCapabilities:
    case protocol::MessageType::kStatus:
    case protocol::MessageType::kTelemetry:
    default:
        return CommandDecision::kInvalidCommand;
    }
}

SessionState SessionPolicy::state() const noexcept
{
    return state_;
}

bool SessionPolicy::authenticated() const noexcept
{
    return authenticated_;
}

bool SessionPolicy::has_session() const noexcept
{
    return state_ == SessionState::kSessionActive ||
        state_ == SessionState::kConfigured ||
        state_ == SessionState::kStreaming;
}

std::uint32_t SessionPolicy::session_id() const noexcept
{
    return session_id_;
}

std::uint16_t SessionPolicy::subscription_mask() const noexcept
{
    return subscription_mask_;
}

} // namespace control
} // namespace telemetry
