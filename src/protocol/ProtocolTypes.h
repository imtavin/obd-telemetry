#pragma once

#include <cstddef>
#include <cstdint>

namespace telemetry
{
namespace protocol
{
constexpr std::uint8_t kProtocolVersion = 1U;
constexpr std::size_t kFrameHeaderSize = 5U;
constexpr std::size_t kCrcSize = 2U;
constexpr std::size_t kMaxPayloadSize = 13U;
constexpr std::size_t kMaxFrameSize = kFrameHeaderSize + kMaxPayloadSize + kCrcSize;
constexpr std::uint8_t kMaxSignalCount = 8U;

// Eight signal bits are reserved by the wire format, but the MVP implements
// only RPM and SPEED. Keeping this mask explicit prevents the protocol from
// accepting signals that the acquisition scheduler cannot provide yet.
constexpr std::uint16_t kRequiredSignalMask = 0x0003U;
constexpr std::uint16_t kSupportedSignalMask = kRequiredSignalMask;
// K-Line has a reserved wire bit, but no K-Line backend exists in the MVP.
// CAPABILITIES must therefore advertise CAN/OBD only.
constexpr std::uint8_t kSupportedBackendMask = 0x01U;

enum class MessageType : std::uint8_t
{
    kHello = 0x01U,
    kGetCapabilities = 0x02U,
    kSetSubscription = 0x03U,
    kStart = 0x04U,
    kStop = 0x05U,
    kPing = 0x06U,
    kGetStatus = 0x07U,
    kAck = 0x80U,
    kError = 0x81U,
    kPong = 0x82U,
    kCapabilities = 0x83U,
    kStatus = 0x84U,
    kTelemetry = 0x85U
};

enum class ErrorDomain : std::uint8_t
{
    kSystem = 0x01U,
    kBle = 0x02U,
    kProtocol = 0x03U,
    kCan = 0x04U,
    kObd = 0x05U,
    kData = 0x06U
};

enum class ErrorCode : std::uint8_t
{
    kInvalidFrame = 0x01U,
    kCrcInvalid = 0x02U,
    kUnknownMessage = 0x03U,
    kInvalidState = 0x04U,
    kUnsupportedVersion = 0x05U,
    kUnsupportedSignal = 0x06U,
    kSubscriptionLimit = 0x07U,
    kCanInitFailed = 0x08U,
    kCanTimeout = 0x09U,
    kCanBusOff = 0x0AU,
    kEcuNegativeResponse = 0x0BU,
    kInvalidEcuResponse = 0x0CU,
    kDataStale = 0x0DU,
    kBleSessionError = 0x0EU
};

enum class ErrorSeverity : std::uint8_t
{
    kInfo = 0x01U,
    kWarning = 0x02U,
    kError = 0x03U,
    kFatal = 0x04U
};

bool is_known_message_type(MessageType type) noexcept;
bool is_command_message_type(MessageType type) noexcept;
bool is_response_message_type(MessageType type) noexcept;
bool is_known_error_domain(ErrorDomain domain) noexcept;
bool is_known_error_code(ErrorCode code) noexcept;
bool is_known_error_severity(ErrorSeverity severity) noexcept;
bool is_supported_signal_mask(std::uint16_t signal_mask) noexcept;
bool is_valid_subscription_mask(std::uint16_t signal_mask) noexcept;
bool is_supported_backend_mask(std::uint8_t backend_mask) noexcept;
bool is_valid_wire_boolean(std::uint8_t value) noexcept;
bool is_compatible_version_range(std::uint8_t minimum_version, std::uint8_t maximum_version) noexcept;

} // namespace protocol
} // namespace telemetry
