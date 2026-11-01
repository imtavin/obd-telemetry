#include "ProtocolMessages.h"

#include <algorithm>

namespace telemetry
{
namespace protocol
{
namespace
{


void initialize_frame(Frame& frame, const MessageType type, const std::uint16_t sequence, const std::uint8_t payload_length) noexcept
{
    frame = Frame{};
    frame.type = type;
    frame.sequence = sequence;
    frame.payload_length = payload_length;
}

void write_u16(std::uint8_t* destination, const std::uint16_t value) noexcept
{
    destination[0U] = static_cast<std::uint8_t>(value & 0x00FFU);
    destination[1U] = static_cast<std::uint8_t>((value >> 8U) & 0x00FFU);
}

void write_u32(std::uint8_t* destination, const std::uint32_t value) noexcept
{
    destination[0U] = static_cast<std::uint8_t>(value & 0x000000FFUL);
    destination[1U] = static_cast<std::uint8_t>((value >> 8U) & 0x000000FFUL);
    destination[2U] = static_cast<std::uint8_t>((value >> 16U) & 0x000000FFUL);
    destination[3U] = static_cast<std::uint8_t>((value >> 24U) & 0x000000FFUL);
}

std::uint16_t read_u16(const std::uint8_t* source) noexcept
{
    return static_cast<std::uint16_t>(source[0U]) |
        static_cast<std::uint16_t>(source[1U]) << 8U;
}

std::uint32_t read_u32(const std::uint8_t* source) noexcept
{
    return static_cast<std::uint32_t>(source[0U]) |
        static_cast<std::uint32_t>(source[1U]) << 8U |
        static_cast<std::uint32_t>(source[2U]) << 16U |
        static_cast<std::uint32_t>(source[3U]) << 24U;
}

bool has_type_and_length(const Frame& frame, const MessageType type, const std::uint8_t length) noexcept
{
    return frame.version == kProtocolVersion && frame.type == type && frame.payload_length == length;
}

bool is_empty_command(const MessageType type) noexcept
{
    return type == MessageType::kGetCapabilities ||
        type == MessageType::kStart ||
        type == MessageType::kStop ||
        type == MessageType::kPing ||
        type == MessageType::kGetStatus;
}
} // namespace

bool build_empty_command(const std::uint16_t sequence, const MessageType type, Frame& frame) noexcept
{
    if (!is_empty_command(type))
    {
        return false;
    }

    initialize_frame(frame, type, sequence, 0U);
    return true;
}

bool build_hello(const std::uint16_t sequence, const HelloPayload& payload, Frame& frame) noexcept
{
    if (!is_compatible_version_range(payload.minimum_version, payload.maximum_version))
    {
        return false;
    }

    initialize_frame(frame, MessageType::kHello, sequence, 6U);
    write_u32(frame.payload.data(), payload.session_id);
    frame.payload[4U] = payload.minimum_version;
    frame.payload[5U] = payload.maximum_version;
    return true;
}

bool decode_hello(const Frame& frame, HelloPayload& payload) noexcept
{
    if (!has_type_and_length(frame, MessageType::kHello, 6U))
    {
        return false;
    }

    payload.session_id = read_u32(frame.payload.data());
    payload.minimum_version = frame.payload[4U];
    payload.maximum_version = frame.payload[5U];
    return is_compatible_version_range(payload.minimum_version, payload.maximum_version);
}

bool build_subscription(const std::uint16_t sequence, const SubscriptionPayload& payload, Frame& frame) noexcept
{
    if (!is_valid_subscription_mask(payload.signal_mask))
    {
        return false;
    }

    initialize_frame(frame, MessageType::kSetSubscription, sequence, 2U);
    write_u16(frame.payload.data(), payload.signal_mask);
    return true;
}

bool decode_subscription(const Frame& frame, SubscriptionPayload& payload) noexcept
{
    if (!has_type_and_length(frame, MessageType::kSetSubscription, 2U))
    {
        return false;
    }

    payload.signal_mask = read_u16(frame.payload.data());
    return is_valid_subscription_mask(payload.signal_mask);
}

bool build_ack(const std::uint16_t sequence, const AckPayload& payload, Frame& frame) noexcept
{
    if (!is_command_message_type(payload.command_type))
    {
        return false;
    }

    initialize_frame(frame, MessageType::kAck, sequence, 8U);
    write_u16(frame.payload.data(), payload.reference_sequence);
    frame.payload[2U] = static_cast<std::uint8_t>(payload.command_type);
    frame.payload[3U] = payload.result;
    write_u32(frame.payload.data() + 4U, payload.session_id);
    return true;
}

bool decode_ack(const Frame& frame, AckPayload& payload) noexcept
{
    if (!has_type_and_length(frame, MessageType::kAck, 8U))
    {
        return false;
    }

    payload.reference_sequence = read_u16(frame.payload.data());
    payload.command_type = static_cast<MessageType>(frame.payload[2U]);
    payload.result = frame.payload[3U];
    payload.session_id = read_u32(frame.payload.data() + 4U);
    return is_command_message_type(payload.command_type);
}

bool build_error(const std::uint16_t sequence, const ErrorPayload& payload, Frame& frame) noexcept
{
    if (payload.context_length > payload.context.size() ||
        !is_known_error_domain(payload.domain) ||
        !is_known_error_code(payload.code) ||
        !is_known_error_severity(payload.severity))
    {
        return false;
    }

    initialize_frame(frame, MessageType::kError, sequence, static_cast<std::uint8_t>(7U + payload.context_length));
    write_u16(frame.payload.data(), payload.reference_sequence);
    frame.payload[2U] = static_cast<std::uint8_t>(payload.domain);
    frame.payload[3U] = static_cast<std::uint8_t>(payload.code);
    frame.payload[4U] = static_cast<std::uint8_t>(payload.severity);
    frame.payload[5U] = payload.recoverable ? 1U : 0U;
    frame.payload[6U] = payload.context_length;
    std::copy_n(payload.context.begin(), payload.context_length, frame.payload.begin() + 7U);
    return true;
}

bool decode_error(const Frame& frame, ErrorPayload& payload) noexcept
{
    if (frame.version != kProtocolVersion || frame.type != MessageType::kError || frame.payload_length < 7U ||
        frame.payload_length > 10U)
    {
        return false;
    }

    const std::uint8_t recoverable = frame.payload[5U];
    payload.reference_sequence = read_u16(frame.payload.data());
    payload.domain = static_cast<ErrorDomain>(frame.payload[2U]);
    payload.code = static_cast<ErrorCode>(frame.payload[3U]);
    payload.severity = static_cast<ErrorSeverity>(frame.payload[4U]);
    payload.recoverable = recoverable != 0U;
    payload.context_length = frame.payload[6U];
    if (!is_known_error_domain(payload.domain) ||
        !is_known_error_code(payload.code) ||
        !is_known_error_severity(payload.severity) ||
        !is_valid_wire_boolean(recoverable) ||
        payload.context_length > payload.context.size() ||
        static_cast<std::size_t>(7U + payload.context_length) != frame.payload_length)
    {
        return false;
    }

    payload.context.fill(0U);
    std::copy_n(frame.payload.begin() + 7U, payload.context_length, payload.context.begin());
    return true;
}

bool build_pong(const std::uint16_t sequence, const PongPayload& payload, Frame& frame) noexcept
{
    initialize_frame(frame, MessageType::kPong, sequence, 2U);
    write_u16(frame.payload.data(), payload.reference_sequence);
    return true;
}

bool decode_pong(const Frame& frame, PongPayload& payload) noexcept
{
    if (!has_type_and_length(frame, MessageType::kPong, 2U))
    {
        return false;
    }

    payload.reference_sequence = read_u16(frame.payload.data());
    return true;
}

bool build_capabilities(const std::uint16_t sequence, const CapabilitiesPayload& payload, Frame& frame) noexcept
{
    if (!is_supported_signal_mask(payload.signal_mask) ||
        !is_supported_backend_mask(payload.backend_mask) ||
        payload.maximum_signals > kMaxSignalCount)
    {
        return false;
    }

    initialize_frame(frame, MessageType::kCapabilities, sequence, 4U);
    write_u16(frame.payload.data(), payload.signal_mask);
    frame.payload[2U] = payload.backend_mask;
    frame.payload[3U] = payload.maximum_signals;
    return true;
}

bool decode_capabilities(const Frame& frame, CapabilitiesPayload& payload) noexcept
{
    if (!has_type_and_length(frame, MessageType::kCapabilities, 4U))
    {
        return false;
    }

    payload.signal_mask = read_u16(frame.payload.data());
    payload.backend_mask = frame.payload[2U];
    payload.maximum_signals = frame.payload[3U];
    return is_supported_signal_mask(payload.signal_mask) &&
        is_supported_backend_mask(payload.backend_mask) &&
        payload.maximum_signals <= kMaxSignalCount;
}

bool build_status(const std::uint16_t sequence, const StatusPayload& payload, Frame& frame) noexcept
{
    if (!is_supported_signal_mask(payload.signal_mask) ||
        (payload.last_error_code != 0U && !is_known_error_code(static_cast<ErrorCode>(payload.last_error_code))))
    {
        return false;
    }

    initialize_frame(frame, MessageType::kStatus, sequence, 7U);
    frame.payload[0U] = payload.device_health;
    frame.payload[1U] = payload.ble_state;
    frame.payload[2U] = payload.session_state;
    frame.payload[3U] = payload.vehicle_state;
    write_u16(frame.payload.data() + 4U, payload.signal_mask);
    frame.payload[6U] = payload.last_error_code;
    return true;
}

bool decode_status(const Frame& frame, StatusPayload& payload) noexcept
{
    if (!has_type_and_length(frame, MessageType::kStatus, 7U))
    {
        return false;
    }

    payload.device_health = frame.payload[0U];
    payload.ble_state = frame.payload[1U];
    payload.session_state = frame.payload[2U];
    payload.vehicle_state = frame.payload[3U];
    payload.signal_mask = read_u16(frame.payload.data() + 4U);
    payload.last_error_code = frame.payload[6U];
    if (!is_supported_signal_mask(payload.signal_mask) ||
        (payload.last_error_code != 0U && !is_known_error_code(static_cast<ErrorCode>(payload.last_error_code))))
    {
        return false;
    }

    return true;
}

bool build_telemetry(const std::uint16_t sequence, const TelemetryPayload& payload, Frame& frame) noexcept
{
    if (!is_supported_signal_mask(payload.valid_mask))
    {
        return false;
    }

    initialize_frame(frame, MessageType::kTelemetry, sequence, 10U);
    write_u32(frame.payload.data(), payload.timestamp_milliseconds);
    write_u16(frame.payload.data() + 4U, payload.valid_mask);
    write_u16(frame.payload.data() + 6U, payload.rpm);
    write_u16(frame.payload.data() + 8U, payload.speed);
    return true;
}

bool decode_telemetry(const Frame& frame, TelemetryPayload& payload) noexcept
{
    if (!has_type_and_length(frame, MessageType::kTelemetry, 10U))
    {
        return false;
    }

    payload.timestamp_milliseconds = read_u32(frame.payload.data());
    payload.valid_mask = read_u16(frame.payload.data() + 4U);
    payload.rpm = read_u16(frame.payload.data() + 6U);
    payload.speed = read_u16(frame.payload.data() + 8U);
    return is_supported_signal_mask(payload.valid_mask);
}

} // namespace protocol
} // namespace telemetry
