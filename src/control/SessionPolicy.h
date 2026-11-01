#pragma once

#include <cstdint>

#include "protocol/ProtocolTypes.h"

namespace telemetry
{
namespace control
{
enum class SessionState : std::uint8_t
{
    kDisconnected = 0U,
    kConnected,
    kSessionActive,
    kConfigured,
    kStreaming
};

enum class CommandDecision : std::uint8_t
{
    kAccepted = 0U,
    kIdempotent,
    kInvalidAuthentication,
    kInvalidState,
    kInvalidCommand,
    kInvalidPayload
};

// Pure domain policy used by ControlTask. It owns no FreeRTOS object and can
// therefore be tested on host before it is wired to BLE callbacks and queues.
class SessionPolicy final
{
public:
    SessionPolicy() noexcept = default;

    void reset() noexcept;
    void on_ble_connected(bool bonded_and_authenticated) noexcept;
    void on_ble_disconnected() noexcept;

    CommandDecision accept_hello(
        std::uint32_t session_id,
        std::uint8_t minimum_version,
        std::uint8_t maximum_version) noexcept;

    CommandDecision handle_command(
        protocol::MessageType type,
        std::uint16_t signal_mask = 0U) noexcept;

    SessionState state() const noexcept;
    bool authenticated() const noexcept;
    bool has_session() const noexcept;
    std::uint32_t session_id() const noexcept;
    std::uint16_t subscription_mask() const noexcept;

private:
    SessionState state_ = SessionState::kDisconnected;
    bool authenticated_ = false;
    std::uint32_t session_id_ = 0U;
    std::uint16_t subscription_mask_ = 0U;
};

} // namespace control
} // namespace telemetry
