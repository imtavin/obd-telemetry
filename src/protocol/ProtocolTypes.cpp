#include "ProtocolTypes.h"

namespace telemetry
{
namespace protocol
{
bool is_known_message_type(const MessageType type) noexcept
{
    switch (type)
    {
    case MessageType::kHello:
    case MessageType::kGetCapabilities:
    case MessageType::kSetSubscription:
    case MessageType::kStart:
    case MessageType::kStop:
    case MessageType::kPing:
    case MessageType::kGetStatus:
    case MessageType::kAck:
    case MessageType::kError:
    case MessageType::kPong:
    case MessageType::kCapabilities:
    case MessageType::kStatus:
    case MessageType::kTelemetry:
        return true;
    default:
        return false;
    }
}

bool is_command_message_type(const MessageType type) noexcept
{
    switch (type)
    {
    case MessageType::kHello:
    case MessageType::kGetCapabilities:
    case MessageType::kSetSubscription:
    case MessageType::kStart:
    case MessageType::kStop:
    case MessageType::kPing:
    case MessageType::kGetStatus:
        return true;
    default:
        return false;
    }
}

bool is_response_message_type(const MessageType type) noexcept
{
    switch (type)
    {
    case MessageType::kAck:
    case MessageType::kError:
    case MessageType::kPong:
    case MessageType::kCapabilities:
    case MessageType::kStatus:
    case MessageType::kTelemetry:
        return true;
    default:
        return false;
    }
}

bool is_known_error_domain(const ErrorDomain domain) noexcept
{
    switch (domain)
    {
    case ErrorDomain::kSystem:
    case ErrorDomain::kBle:
    case ErrorDomain::kProtocol:
    case ErrorDomain::kCan:
    case ErrorDomain::kObd:
    case ErrorDomain::kData:
        return true;
    default:
        return false;
    }
}

bool is_known_error_code(const ErrorCode code) noexcept
{
    switch (code)
    {
    case ErrorCode::kInvalidFrame:
    case ErrorCode::kCrcInvalid:
    case ErrorCode::kUnknownMessage:
    case ErrorCode::kInvalidState:
    case ErrorCode::kUnsupportedVersion:
    case ErrorCode::kUnsupportedSignal:
    case ErrorCode::kSubscriptionLimit:
    case ErrorCode::kCanInitFailed:
    case ErrorCode::kCanTimeout:
    case ErrorCode::kCanBusOff:
    case ErrorCode::kEcuNegativeResponse:
    case ErrorCode::kInvalidEcuResponse:
    case ErrorCode::kDataStale:
    case ErrorCode::kBleSessionError:
        return true;
    default:
        return false;
    }
}

bool is_known_error_severity(const ErrorSeverity severity) noexcept
{
    switch (severity)
    {
    case ErrorSeverity::kInfo:
    case ErrorSeverity::kWarning:
    case ErrorSeverity::kError:
    case ErrorSeverity::kFatal:
        return true;
    default:
        return false;
    }
}

bool is_supported_signal_mask(const std::uint16_t signal_mask) noexcept
{
    return (signal_mask & static_cast<std::uint16_t>(~kSupportedSignalMask)) == 0U;
}

bool is_valid_subscription_mask(const std::uint16_t signal_mask) noexcept
{
    return signal_mask == kRequiredSignalMask;
}

bool is_supported_backend_mask(const std::uint8_t backend_mask) noexcept
{
    return backend_mask != 0U &&
        (backend_mask & static_cast<std::uint8_t>(~kSupportedBackendMask)) == 0U;
}

bool is_valid_wire_boolean(const std::uint8_t value) noexcept
{
    return value == 0U || value == 1U;
}

bool is_compatible_version_range(
    const std::uint8_t minimum_version,
    const std::uint8_t maximum_version) noexcept
{
    return minimum_version <= maximum_version &&
        minimum_version <= kProtocolVersion &&
        kProtocolVersion <= maximum_version;
}

} // namespace protocol
} // namespace telemetry
