#pragma once

#include <array>
#include <cstdint>

#include "Frame.h"

namespace telemetry
{
namespace protocol
{
struct HelloPayload
{
    std::uint32_t session_id = 0U;
    std::uint8_t minimum_version = kProtocolVersion;
    std::uint8_t maximum_version = kProtocolVersion;
};

struct SubscriptionPayload
{
    std::uint16_t signal_mask = 0U;
};

struct AckPayload
{
    std::uint16_t reference_sequence = 0U;
    MessageType command_type = MessageType::kHello;
    std::uint8_t result = 0U;
    std::uint32_t session_id = 0U;
};

struct ErrorPayload
{
    std::uint16_t reference_sequence = 0U;
    ErrorDomain domain = ErrorDomain::kSystem;
    ErrorCode code = ErrorCode::kInvalidFrame;
    ErrorSeverity severity = ErrorSeverity::kError;
    bool recoverable = false;
    std::uint8_t context_length = 0U;
    std::array<std::uint8_t, 3U> context{};
};

struct PongPayload
{
    std::uint16_t reference_sequence = 0U;
};

struct CapabilitiesPayload
{
    std::uint16_t signal_mask = 0U;
    std::uint8_t backend_mask = 0U;
    std::uint8_t maximum_signals = 0U;
};

struct StatusPayload
{
    std::uint8_t device_health = 0U;
    std::uint8_t ble_state = 0U;
    std::uint8_t session_state = 0U;
    std::uint8_t vehicle_state = 0U;
    std::uint16_t signal_mask = 0U;
    std::uint8_t last_error_code = 0U;
};

struct TelemetryPayload
{
    std::uint32_t timestamp_milliseconds = 0U;
    std::uint16_t valid_mask = 0U;
    std::uint16_t rpm = 0U;
    std::uint16_t speed = 0U;
};

bool build_empty_command(std::uint16_t sequence, MessageType type, Frame& frame) noexcept;
bool build_hello(std::uint16_t sequence, const HelloPayload& payload, Frame& frame) noexcept;
bool decode_hello(const Frame& frame, HelloPayload& payload) noexcept;
bool build_subscription(std::uint16_t sequence, const SubscriptionPayload& payload, Frame& frame) noexcept;
bool decode_subscription(const Frame& frame, SubscriptionPayload& payload) noexcept;
bool build_ack(std::uint16_t sequence, const AckPayload& payload, Frame& frame) noexcept;
bool decode_ack(const Frame& frame, AckPayload& payload) noexcept;
bool build_error(std::uint16_t sequence, const ErrorPayload& payload, Frame& frame) noexcept;
bool decode_error(const Frame& frame, ErrorPayload& payload) noexcept;
bool build_pong(std::uint16_t sequence, const PongPayload& payload, Frame& frame) noexcept;
bool decode_pong(const Frame& frame, PongPayload& payload) noexcept;
bool build_capabilities(std::uint16_t sequence, const CapabilitiesPayload& payload, Frame& frame) noexcept;
bool decode_capabilities(const Frame& frame, CapabilitiesPayload& payload) noexcept;
bool build_status(std::uint16_t sequence, const StatusPayload& payload, Frame& frame) noexcept;
bool decode_status(const Frame& frame, StatusPayload& payload) noexcept;
bool build_telemetry(std::uint16_t sequence, const TelemetryPayload& payload, Frame& frame) noexcept;
bool decode_telemetry(const Frame& frame, TelemetryPayload& payload) noexcept;

} // namespace protocol
} // namespace telemetry
