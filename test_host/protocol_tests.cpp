#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "control/SessionPolicy.h"
#include "protocol/Crc16.h"
#include "protocol/ProtocolEncoder.h"
#include "protocol/ProtocolMessages.h"
#include "protocol/ProtocolParser.h"
#include "protocol/Sequence.h"

namespace
{
using telemetry::control::CommandDecision;
using telemetry::control::SessionPolicy;
using telemetry::control::SessionState;
using telemetry::protocol::AckPayload;
using telemetry::protocol::CapabilitiesPayload;
using telemetry::protocol::Crc16;
using telemetry::protocol::EncodeStatus;
using telemetry::protocol::ErrorCode;
using telemetry::protocol::ErrorDomain;
using telemetry::protocol::ErrorPayload;
using telemetry::protocol::ErrorSeverity;
using telemetry::protocol::Frame;
using telemetry::protocol::HelloPayload;
using telemetry::protocol::MessageType;
using telemetry::protocol::ParseStatus;
using telemetry::protocol::ProtocolEncoder;
using telemetry::protocol::ProtocolParser;
using telemetry::protocol::SequenceGenerator;
using telemetry::protocol::SequenceStatus;
using telemetry::protocol::SequenceTracker;
using telemetry::protocol::StatusPayload;
using telemetry::protocol::SubscriptionPayload;
using telemetry::protocol::TelemetryPayload;

bool check(const bool condition, const char* expression, const char* file, const int line)
{
    if (condition)
    {
        return true;
    }

    std::cerr << file << ":" << line << " check failed: " << expression << std::endl;
    return false;
}

bool parse_frame(
    const std::array<std::uint8_t, telemetry::protocol::kMaxFrameSize>& bytes,
    const std::size_t size,
    Frame& frame,
    const std::uint32_t start_time_ms = 0U)
{
    ProtocolParser parser;
    ParseStatus status = ParseStatus::kInProgress;

    for (std::size_t index = 0U; index < size; ++index)
    {
        status = parser.consume(bytes[index], frame, start_time_ms + static_cast<std::uint32_t>(index));
    }

    return status == ParseStatus::kFrameReady;
}

bool encode_frame(
    const Frame& frame,
    std::array<std::uint8_t, telemetry::protocol::kMaxFrameSize>& encoded,
    std::size_t& encoded_size)
{
    return ProtocolEncoder::encode(frame, encoded, encoded_size) == EncodeStatus::kOk;
}

bool test_crc_vectors()
{
    const char input[] = "123456789";
    if (!check(Crc16::calculate(reinterpret_cast<const std::uint8_t*>(input), 9U) == 0x29B1U,
                                "CRC known vector", __FILE__, __LINE__)) return false;
    if (!check(Crc16::calculate(nullptr, 0U) == 0xFFFFU, "CRC empty payload", __FILE__, __LINE__)) return false;

    const std::array<std::uint8_t, 13U> maximum_payload = {
        3U, 20U, 37U, 54U, 71U, 88U, 105U, 122U, 139U, 156U, 173U, 190U, 207U};
    if (!check(Crc16::calculate(maximum_payload.data(), maximum_payload.size()) == 0xED17U,
               "CRC maximum payload", __FILE__, __LINE__)) return false;

    const std::array<std::uint8_t, 7U> random_payload = {0x00U, 0xFFU, 0x12U, 0xA5U, 0x5AU, 0x80U, 0x01U};
    if (!check(Crc16::calculate(random_payload.data(), random_payload.size()) == 0x212DU,
               "CRC arbitrary payload", __FILE__, __LINE__)) return false;
    return true;
}

bool test_hello_round_trip_and_version_negotiation()
{
    HelloPayload source_payload;
    source_payload.session_id = 0x12345678UL;
    source_payload.minimum_version = 1U;
    source_payload.maximum_version = 1U;
    Frame source_frame;
    if (!check(telemetry::protocol::build_hello(0x1234U, source_payload, source_frame),
               "build HELLO", __FILE__, __LINE__)) return false;

    std::array<std::uint8_t, telemetry::protocol::kMaxFrameSize> encoded{};
    std::size_t encoded_size = 0U;
    if (!check(encode_frame(source_frame, encoded, encoded_size), "encode HELLO", __FILE__, __LINE__)) return false;
    if (!check(encoded_size == 13U, "HELLO frame size", __FILE__, __LINE__)) return false;
    if (!check(encoded[2U] == 0x34U && encoded[3U] == 0x12U, "sequence little endian", __FILE__, __LINE__)) return false;
    if (!check(encoded[5U] == 0x78U && encoded[8U] == 0x12U, "session little endian", __FILE__, __LINE__)) return false;

    Frame parsed_frame;
    if (!check(parse_frame(encoded, encoded_size, parsed_frame), "parse HELLO", __FILE__, __LINE__)) return false;
    HelloPayload parsed_payload;
    if (!check(telemetry::protocol::decode_hello(parsed_frame, parsed_payload),
               "decode HELLO", __FILE__, __LINE__)) return false;
    if (!check(parsed_payload.session_id == source_payload.session_id, "HELLO session", __FILE__, __LINE__)) return false;

    source_payload.minimum_version = 2U;
    source_payload.maximum_version = 3U;
    if (!check(!telemetry::protocol::build_hello(1U, source_payload, source_frame),
               "reject incompatible HELLO", __FILE__, __LINE__)) return false;
    return true;
}

bool test_empty_commands_and_maximum_frame()
{
    const std::array<MessageType, 5U> empty_commands = {
        MessageType::kGetCapabilities,
        MessageType::kStart,
        MessageType::kStop,
        MessageType::kPing,
        MessageType::kGetStatus};

    for (const MessageType type : empty_commands)
    {
        Frame frame;
        if (!check(telemetry::protocol::build_empty_command(9U, type, frame),
                   "build empty command", __FILE__, __LINE__)) return false;
        std::array<std::uint8_t, telemetry::protocol::kMaxFrameSize> encoded{};
        std::size_t encoded_size = 0U;
        if (!check(encode_frame(frame, encoded, encoded_size), "encode empty command", __FILE__, __LINE__)) return false;
        if (!check(encoded_size == telemetry::protocol::kFrameHeaderSize + telemetry::protocol::kCrcSize,
                   "empty command size", __FILE__, __LINE__)) return false;
        Frame parsed;
        if (!check(parse_frame(encoded, encoded_size, parsed), "parse empty command", __FILE__, __LINE__)) return false;
        if (!check(parsed.type == type && parsed.payload_length == 0U,
                   "empty command fields", __FILE__, __LINE__)) return false;
    }

    Frame maximum_frame;
    maximum_frame.type = MessageType::kError;
    maximum_frame.sequence = 0xFFFFU;
    maximum_frame.payload_length = static_cast<std::uint8_t>(telemetry::protocol::kMaxPayloadSize);
    for (std::size_t index = 0U; index < telemetry::protocol::kMaxPayloadSize; ++index)
    {
        maximum_frame.payload[index] = static_cast<std::uint8_t>(index + 1U);
    }

    std::array<std::uint8_t, telemetry::protocol::kMaxFrameSize> encoded{};
    std::size_t encoded_size = 0U;
    if (!check(encode_frame(maximum_frame, encoded, encoded_size),
               "encode maximum frame", __FILE__, __LINE__)) return false;
    if (!check(encoded_size == telemetry::protocol::kMaxFrameSize,
               "maximum frame size", __FILE__, __LINE__)) return false;
    Frame parsed;
    if (!check(parse_frame(encoded, encoded_size, parsed), "parse maximum frame", __FILE__, __LINE__)) return false;
    if (!check(parsed.payload == maximum_frame.payload, "maximum payload", __FILE__, __LINE__)) return false;
    return true;
}

bool test_telemetry_fragmented_round_trip()
{
    TelemetryPayload source_payload;
    source_payload.timestamp_milliseconds = 123456U;
    source_payload.valid_mask = telemetry::protocol::kRequiredSignalMask;
    source_payload.rpm = 2500U;
    source_payload.speed = 80U;
    Frame source_frame;
    if (!check(telemetry::protocol::build_telemetry(7U, source_payload, source_frame),
               "build telemetry", __FILE__, __LINE__)) return false;

    std::array<std::uint8_t, telemetry::protocol::kMaxFrameSize> encoded{};
    std::size_t encoded_size = 0U;
    if (!check(encode_frame(source_frame, encoded, encoded_size), "encode telemetry", __FILE__, __LINE__)) return false;

    ProtocolParser parser;
    Frame parsed_frame;
    ParseStatus status = ParseStatus::kInProgress;
    for (std::size_t index = 0U; index < encoded_size; ++index)
    {
        status = parser.consume(encoded[index], parsed_frame, static_cast<std::uint32_t>(index));
        if (!check(index + 1U == encoded_size || status == ParseStatus::kInProgress,
                   "fragment stays in progress", __FILE__, __LINE__)) return false;
    }
    if (!check(status == ParseStatus::kFrameReady, "fragment complete", __FILE__, __LINE__)) return false;

    TelemetryPayload parsed_payload;
    if (!check(telemetry::protocol::decode_telemetry(parsed_frame, parsed_payload),
               "decode telemetry", __FILE__, __LINE__)) return false;
    if (!check(parsed_payload.timestamp_milliseconds == source_payload.timestamp_milliseconds &&
                   parsed_payload.rpm == source_payload.rpm && parsed_payload.speed == source_payload.speed,
               "telemetry fields", __FILE__, __LINE__)) return false;
    return true;
}

bool test_parser_rejects_invalid_input_and_resynchronizes()
{
    ProtocolParser parser;
    Frame frame;
    if (!check(parser.consume(2U, frame, 0U) == ParseStatus::kInvalidVersion,
               "invalid version", __FILE__, __LINE__)) return false;
    if (!check(parser.consume(1U, frame, 1U) == ParseStatus::kInProgress,
               "new frame after reset", __FILE__, __LINE__)) return false;
    if (!check(parser.consume(0x99U, frame, 2U) == ParseStatus::kUnknownMessageType,
               "unknown type", __FILE__, __LINE__)) return false;

    const std::array<std::uint8_t, 5U> invalid_length = {1U, 0x01U, 0U, 0U, 14U};
    ParseStatus status = ParseStatus::kInProgress;
    for (std::size_t index = 0U; index < invalid_length.size(); ++index)
    {
        status = parser.consume(invalid_length[index], frame, static_cast<std::uint32_t>(index + 3U));
    }
    if (!check(status == ParseStatus::kInvalidLength, "invalid length", __FILE__, __LINE__)) return false;

    Frame ping;
    if (!check(telemetry::protocol::build_empty_command(1U, MessageType::kPing, ping),
               "build ping", __FILE__, __LINE__)) return false;
    std::array<std::uint8_t, telemetry::protocol::kMaxFrameSize> invalid_crc{};
    std::size_t invalid_crc_size = 0U;
    if (!check(encode_frame(ping, invalid_crc, invalid_crc_size), "encode ping", __FILE__, __LINE__)) return false;
    invalid_crc[invalid_crc_size - 1U] ^= 0xFFU;
    for (std::size_t index = 0U; index < invalid_crc_size; ++index)
    {
        status = parser.consume(invalid_crc[index], frame, static_cast<std::uint32_t>(20U + index));
    }
    if (!check(status == ParseStatus::kInvalidCrc, "invalid CRC", __FILE__, __LINE__)) return false;

    std::array<std::uint8_t, telemetry::protocol::kMaxFrameSize> valid_frame{};
    std::size_t valid_size = 0U;
    if (!check(encode_frame(ping, valid_frame, valid_size), "encode recovery ping", __FILE__, __LINE__)) return false;
    for (std::size_t index = 0U; index < valid_size; ++index)
    {
        status = parser.consume(valid_frame[index], frame, static_cast<std::uint32_t>(40U + index));
    }
    if (!check(status == ParseStatus::kFrameReady, "parser recovers at next frame boundary", __FILE__, __LINE__)) return false;
    return true;
}

bool test_parser_timeout()
{
    HelloPayload payload;
    payload.session_id = 4U;
    Frame frame;
    if (!check(telemetry::protocol::build_hello(1U, payload, frame), "build timeout HELLO", __FILE__, __LINE__)) return false;
    std::array<std::uint8_t, telemetry::protocol::kMaxFrameSize> encoded{};
    std::size_t encoded_size = 0U;
    if (!check(encode_frame(frame, encoded, encoded_size), "encode timeout HELLO", __FILE__, __LINE__)) return false;

    ProtocolParser parser;
    Frame parsed;
    for (std::size_t index = 0U; index < 4U; ++index)
    {
        if (!check(parser.consume(encoded[index], parsed, 100U + static_cast<std::uint32_t>(index)) == ParseStatus::kInProgress,
                   "partial frame", __FILE__, __LINE__)) return false;
    }
    if (!check(parser.has_partial_frame(), "parser reports partial frame", __FILE__, __LINE__)) return false;
    if (!check(parser.expire(110U, 10U) == ParseStatus::kInProgress,
               "timeout not reached", __FILE__, __LINE__)) return false;
    if (!check(parser.expire(114U, 10U) == ParseStatus::kTimeout,
               "partial frame timeout", __FILE__, __LINE__)) return false;
    if (!check(!parser.has_partial_frame(), "timeout resets parser", __FILE__, __LINE__)) return false;
    return true;
}

bool test_typed_message_validation()
{
    SubscriptionPayload subscription;
    subscription.signal_mask = telemetry::protocol::kRequiredSignalMask;
    Frame frame;
    if (!check(telemetry::protocol::build_subscription(2U, subscription, frame),
               "build subscription", __FILE__, __LINE__)) return false;
    SubscriptionPayload decoded_subscription;
    if (!check(telemetry::protocol::decode_subscription(frame, decoded_subscription),
               "decode subscription", __FILE__, __LINE__)) return false;

    subscription.signal_mask = 0x0004U;
    if (!check(!telemetry::protocol::build_subscription(2U, subscription, frame),
               "reject unsupported signal", __FILE__, __LINE__)) return false;

    subscription.signal_mask = 0x0000U;
    if (!check(!telemetry::protocol::build_subscription(2U, subscription, frame),
               "reject empty subscription", __FILE__, __LINE__)) return false;

    subscription.signal_mask = 0x0001U;
    if (!check(!telemetry::protocol::build_subscription(2U, subscription, frame),
               "reject partial subscription", __FILE__, __LINE__)) return false;

    subscription.signal_mask = telemetry::protocol::kRequiredSignalMask;
    if (!check(telemetry::protocol::build_subscription(2U, subscription, frame),
               "rebuild subscription", __FILE__, __LINE__)) return false;
    frame.payload[0U] = 0x01U;
    frame.payload[1U] = 0x00U;
    if (!check(!telemetry::protocol::decode_subscription(frame, decoded_subscription),
               "reject decoded partial subscription", __FILE__, __LINE__)) return false;

    CapabilitiesPayload capabilities;
    capabilities.signal_mask = telemetry::protocol::kRequiredSignalMask;
    capabilities.backend_mask = telemetry::protocol::kSupportedBackendMask;
    capabilities.maximum_signals = telemetry::protocol::kMaxSignalCount;
    if (!check(telemetry::protocol::build_capabilities(1U, capabilities, frame),
               "build capabilities", __FILE__, __LINE__)) return false;
    capabilities.backend_mask = 0x04U;
    if (!check(!telemetry::protocol::build_capabilities(1U, capabilities, frame),
               "reject unsupported backend", __FILE__, __LINE__)) return false;

    capabilities.backend_mask = 0x00U;
    if (!check(!telemetry::protocol::build_capabilities(1U, capabilities, frame),
               "reject empty backend", __FILE__, __LINE__)) return false;

    capabilities.backend_mask = 0x02U;
    if (!check(!telemetry::protocol::build_capabilities(1U, capabilities, frame),
               "reject K-Line backend before implementation", __FILE__, __LINE__)) return false;

    ErrorPayload error;
    error.domain = ErrorDomain::kProtocol;
    error.code = ErrorCode::kInvalidFrame;
    error.severity = ErrorSeverity::kError;
    error.context_length = 2U;
    error.context[0U] = 0xAAU;
    error.context[1U] = 0x55U;
    if (!check(telemetry::protocol::build_error(1U, error, frame),
               "build error", __FILE__, __LINE__)) return false;
    frame.payload[2U] = 0xFFU;
    if (!check(!telemetry::protocol::decode_error(frame, error),
               "reject unknown error domain", __FILE__, __LINE__)) return false;

    error.domain = ErrorDomain::kProtocol;
    if (!check(telemetry::protocol::build_error(1U, error, frame),
               "rebuild error", __FILE__, __LINE__)) return false;
    frame.payload[5U] = 2U;
    if (!check(!telemetry::protocol::decode_error(frame, error),
               "reject invalid boolean", __FILE__, __LINE__)) return false;

    TelemetryPayload telemetry;
    telemetry.valid_mask = 0x0004U;
    if (!check(!telemetry::protocol::build_telemetry(1U, telemetry, frame),
               "reject unsupported telemetry bit", __FILE__, __LINE__)) return false;
    return true;
}

bool test_sequence_generation_and_tracking()
{
    SequenceGenerator generator(0xFFFEU);
    if (!check(generator.next() == 0xFFFEU && generator.next() == 0xFFFFU && generator.next() == 0x0000U,
               "sequence generator wrap", __FILE__, __LINE__)) return false;

    SequenceTracker tracker;
    if (!check(tracker.observe(0xFFFEU) == SequenceStatus::kFirst, "first sequence", __FILE__, __LINE__)) return false;
    if (!check(tracker.observe(0xFFFFU) == SequenceStatus::kExpected, "expected sequence", __FILE__, __LINE__)) return false;
    if (!check(tracker.observe(0x0000U) == SequenceStatus::kExpected, "wrapped sequence", __FILE__, __LINE__)) return false;
    if (!check(tracker.observe(0x0002U) == SequenceStatus::kGap, "sequence gap", __FILE__, __LINE__)) return false;
    if (!check(tracker.observe(0x0002U) == SequenceStatus::kDuplicate, "duplicate sequence", __FILE__, __LINE__)) return false;
    if (!check(tracker.observe(0x0001U) == SequenceStatus::kOutOfOrder, "out of order sequence", __FILE__, __LINE__)) return false;
    if (!check(tracker.last_sequence() == 0x0002U, "out of order does not replace baseline", __FILE__, __LINE__)) return false;
    return true;
}

bool test_session_policy()
{
    SessionPolicy policy;
    if (!check(policy.handle_command(MessageType::kStart) == CommandDecision::kInvalidAuthentication,
               "command before authentication", __FILE__, __LINE__)) return false;

    policy.on_ble_connected(false);
    if (!check(policy.accept_hello(7U, 1U, 1U) == CommandDecision::kInvalidAuthentication,
               "HELLO before authentication", __FILE__, __LINE__)) return false;

    policy.on_ble_connected(true);
    if (!check(policy.accept_hello(7U, 2U, 3U) == CommandDecision::kInvalidPayload,
               "incompatible session version", __FILE__, __LINE__)) return false;
    if (!check(policy.accept_hello(7U, 1U, 1U) == CommandDecision::kAccepted,
               "valid HELLO", __FILE__, __LINE__)) return false;
    if (!check(policy.state() == SessionState::kSessionActive && policy.has_session(),
               "active session", __FILE__, __LINE__)) return false;
    if (!check(policy.handle_command(MessageType::kStart) == CommandDecision::kInvalidState,
               "START before subscription", __FILE__, __LINE__)) return false;
    if (!check(policy.handle_command(MessageType::kSetSubscription, telemetry::protocol::kRequiredSignalMask) ==
                   CommandDecision::kAccepted,
               "valid subscription", __FILE__, __LINE__)) return false;
    if (!check(policy.handle_command(MessageType::kSetSubscription, telemetry::protocol::kRequiredSignalMask) ==
                   CommandDecision::kIdempotent,
               "idempotent subscription", __FILE__, __LINE__)) return false;
    if (!check(policy.handle_command(MessageType::kStart) == CommandDecision::kAccepted,
               "START after subscription", __FILE__, __LINE__)) return false;
    if (!check(policy.handle_command(MessageType::kStart) == CommandDecision::kIdempotent,
               "idempotent START", __FILE__, __LINE__)) return false;
    if (!check(policy.handle_command(MessageType::kSetSubscription, telemetry::protocol::kRequiredSignalMask) ==
                   CommandDecision::kInvalidState,
               "subscription change while streaming", __FILE__, __LINE__)) return false;
    if (!check(policy.handle_command(MessageType::kStop) == CommandDecision::kAccepted,
               "STOP streaming", __FILE__, __LINE__)) return false;
    if (!check(policy.handle_command(MessageType::kStop) == CommandDecision::kIdempotent,
               "idempotent STOP", __FILE__, __LINE__)) return false;
    if (!check(policy.handle_command(MessageType::kTelemetry) == CommandDecision::kInvalidCommand,
               "reject response as command", __FILE__, __LINE__)) return false;

    policy.on_ble_disconnected();
    if (!check(policy.state() == SessionState::kDisconnected && !policy.has_session(),
               "disconnect resets session", __FILE__, __LINE__)) return false;
    return true;
}

bool test_encoder_rejects_invalid_frame()
{
    Frame frame;
    std::array<std::uint8_t, telemetry::protocol::kMaxFrameSize> encoded{};
    std::size_t encoded_size = 99U;
    frame.version = 2U;
    if (!check(ProtocolEncoder::encode(frame, encoded, encoded_size) == EncodeStatus::kInvalidVersion,
               "invalid encode version", __FILE__, __LINE__)) return false;
    if (!check(encoded_size == 0U, "invalid encode size", __FILE__, __LINE__)) return false;
    frame.version = telemetry::protocol::kProtocolVersion;
    frame.payload_length = 14U;
    if (!check(ProtocolEncoder::encode(frame, encoded, encoded_size) == EncodeStatus::kPayloadTooLarge,
               "oversized payload", __FILE__, __LINE__)) return false;
    frame.payload_length = 0U;
    frame.type = static_cast<MessageType>(0x99U);
    if (!check(ProtocolEncoder::encode(frame, encoded, encoded_size) == EncodeStatus::kUnknownMessageType,
               "unknown encode type", __FILE__, __LINE__)) return false;
    return true;
}

bool run_test(const char* name, bool (*test)())
{
    const bool passed = test();
    std::cout << (passed ? "PASS " : "FAIL ") << name << std::endl;
    return passed;
}
} // namespace

int main()
{
    int failures = 0;
    failures += run_test("crc_vectors", test_crc_vectors) ? 0 : 1;
    failures += run_test("hello_round_trip_and_version_negotiation", test_hello_round_trip_and_version_negotiation) ? 0 : 1;
    failures += run_test("empty_commands_and_maximum_frame", test_empty_commands_and_maximum_frame) ? 0 : 1;
    failures += run_test("telemetry_fragmented_round_trip", test_telemetry_fragmented_round_trip) ? 0 : 1;
    failures += run_test("parser_invalid_input_and_resynchronization", test_parser_rejects_invalid_input_and_resynchronizes) ? 0 : 1;
    failures += run_test("parser_timeout", test_parser_timeout) ? 0 : 1;
    failures += run_test("typed_message_validation", test_typed_message_validation) ? 0 : 1;
    failures += run_test("sequence_generation_and_tracking", test_sequence_generation_and_tracking) ? 0 : 1;
    failures += run_test("session_policy", test_session_policy) ? 0 : 1;
    failures += run_test("encoder_rejects_invalid_frame", test_encoder_rejects_invalid_frame) ? 0 : 1;
    return failures == 0 ? 0 : 1;
}
