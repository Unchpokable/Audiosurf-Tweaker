#pragma once

#include <cstdint>
#include <exception>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace as::proto
{
class asbridge_msg_parse_error final : public std::exception {
public:
    explicit asbridge_msg_parse_error(std::string message) noexcept : m_message(std::move(message))
    {
    }

    [[nodiscard]] virtual const char* what() const noexcept override
    {
        return m_message.c_str();
    }

private:
    std::string m_message;
};

enum class asbridge_msg_header : std::uint8_t {
    client_command,
    server_report,
};

enum class asbridge_msg_type : std::uint8_t {
    send,
    overlay_send,
    ok,
    failed,
    overlay_ok,
    overlay_failed,
    broadcast_forward,
    overlay_forward,
    service,
};

struct asbridge_msg final {
    asbridge_msg_header header;
    asbridge_msg_type msg;
    std::vector<std::string> details;
};
} // namespace as::proto

namespace as::proto::rules
{
inline constexpr auto message_header_client_command = "CCOMMAND";
inline constexpr auto message_header_server_report = "SREPORT";

inline constexpr auto message_msg_send = "SEND";
inline constexpr auto message_msg_overlay_send = "OVERLAY_SEND";
inline constexpr auto message_msg_ok = "OK";
inline constexpr auto message_msg_failed = "FAILED";
inline constexpr auto message_msg_overlay_ok = "OVERLAY_OK";
inline constexpr auto message_msg_overlay_failed = "OVERLAY_FAILED";
inline constexpr auto message_msg_broadcast_forward = "BROADCAST_FORWARD";
inline constexpr auto message_msg_overlay_forward = "OVERLAY_FORWARD";
inline constexpr auto message_msg_service = "SERVICE";

inline constexpr auto service_status_window_lost = "WINDOW_LOST";
inline constexpr auto service_status_window_found = "WINDOW_FOUND";

/// Empty for a value outside the enum - which is exactly what the parser produces for an
/// unrecognised token, so "did this round-trip to a name" doubles as "is this a known value".
constexpr std::string_view to_string(asbridge_msg_header header) noexcept
{
    switch(header) {
        case asbridge_msg_header::client_command:
            return message_header_client_command;
        case asbridge_msg_header::server_report:
            return message_header_server_report;
    }

    return {};
}

constexpr std::string_view to_string(asbridge_msg_type msg) noexcept
{
    switch(msg) {
        case asbridge_msg_type::send:
            return message_msg_send;
        case asbridge_msg_type::overlay_send:
            return message_msg_overlay_send;
        case asbridge_msg_type::ok:
            return message_msg_ok;
        case asbridge_msg_type::failed:
            return message_msg_failed;
        case asbridge_msg_type::overlay_ok:
            return message_msg_overlay_ok;
        case asbridge_msg_type::overlay_failed:
            return message_msg_overlay_failed;
        case asbridge_msg_type::broadcast_forward:
            return message_msg_broadcast_forward;
        case asbridge_msg_type::overlay_forward:
            return message_msg_overlay_forward;
        case asbridge_msg_type::service:
            return message_msg_service;
    }

    return {};
}

enum class message_shape : std::uint8_t {
    /// Not a message this protocol defines in this direction.
    undefined,
    details_optional,
    details_required,
};

/// The whole grammar above the token level: which (header, msg) pairs exist and which of them must
/// carry details. Direction is part of the meaning - a SREPORT SEND is not a message with a bad
/// field, it is a message going the wrong way - so anything unlisted is `undefined`, not merely odd.
constexpr message_shape shape_of(asbridge_msg_header header, asbridge_msg_type msg) noexcept
{
    switch(header) {
        case asbridge_msg_header::client_command:
            switch(msg) {
                case asbridge_msg_type::send:
                case asbridge_msg_type::overlay_send:
                    return message_shape::details_required;
                default:
                    return message_shape::undefined;
            }

        case asbridge_msg_header::server_report:
            switch(msg) {
                case asbridge_msg_type::ok:
                case asbridge_msg_type::overlay_ok:
                    return message_shape::details_optional;
                case asbridge_msg_type::failed:
                case asbridge_msg_type::overlay_failed:
                case asbridge_msg_type::broadcast_forward:
                case asbridge_msg_type::overlay_forward:
                case asbridge_msg_type::service:
                    return message_shape::details_required;
                default:
                    return message_shape::undefined;
            }
    }

    return message_shape::undefined;
}
} // namespace as::proto::rules

namespace as::proto
{
std::expected<asbridge_msg, asbridge_msg_parse_error> parse_message(const std::string& raw_msg);
std::string serialize_message(const asbridge_msg& msg);
} // namespace as::proto
