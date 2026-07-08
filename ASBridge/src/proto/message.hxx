#pragma once

#include "models/vd_static_model.hxx"

#include <vd.hxx>

#include <cstdint>
#include <exception>
#include <expected>
#include <string>
#include <string_view>

namespace as::proto
{
class asbridge_msg_parse_error final : public std::exception {
public:
    explicit asbridge_msg_parse_error(std::string message) noexcept : m_message(std::move(message))
    {
    }

    [[nodiscard]] const char* what() const noexcept override
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
    ok,
    failed,
    broadcast_forward,
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
inline constexpr auto message_msg_ok = "OK";
inline constexpr auto message_msg_failed = "FAILED";
inline constexpr auto message_msg_broadcast_forward = "BROADCAST_FORWARD";

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
        case asbridge_msg_type::ok:
            return message_msg_ok;
        case asbridge_msg_type::failed:
            return message_msg_failed;
        case asbridge_msg_type::broadcast_forward:
            return message_msg_broadcast_forward;
    }

    return {};
}

struct asbridge_msg_header_known final {
    static constexpr auto description = "header must be one of the known values";

    vd::result operator()(const asbridge_msg& msg) const noexcept
    {
        if(!to_string(msg.header).empty()) {
            return vd::result::ok();
        }

        return vd::result::failed({ std::format("{}. Invalid header: {}", description, static_cast<int>(msg.header)) });
    }
};

struct asbridge_msg_msg_known final {
    static constexpr auto description = "msg must be one of the known values";

    vd::result operator()(const asbridge_msg& msg) const noexcept
    {
        if(!to_string(msg.msg).empty()) {
            return vd::result::ok();
        }

        return vd::result::failed({ std::format("{}. Invalid msg: {}", description, static_cast<int>(msg.msg)) });
    }
};

struct asbridge_msg_failed_details_non_empty final {
    static constexpr auto description = "details must be non-empty";

    vd::result operator()(const asbridge_msg& msg) const noexcept
    {
        if(msg.msg == asbridge_msg_type::failed && msg.details.empty()) {
            return vd::result::failed({ std::format("{}. details must be non-empty when msg is FAILED", description) });
        }

        return vd::result::ok();
    }
};

struct asbridge_msg_broadcast_forward_details_non_empty final {
    static constexpr auto description = "details must be non-empty";

    vd::result operator()(const asbridge_msg& msg) const noexcept
    {
        if(msg.msg == asbridge_msg_type::broadcast_forward && msg.details.empty()) {
            return vd::result::failed({ std::format("{}. details must be non-empty when msg is BROADCAST_FORWARD", description) });
        }

        return vd::result::ok();
    }
};

struct asbridge_msg_client_command_send final {
    static constexpr auto description = "header must be CCOMMAND and msg must be SEND";

    vd::result operator()(const asbridge_msg& msg) const noexcept
    {
        if(msg.header == asbridge_msg_header::client_command && msg.msg == asbridge_msg_type::send) {
            return vd::result::ok();
        }

        return vd::result::failed(
            { std::format("{}. Invalid header/msg combination: {}/{}", description, to_string(msg.header), to_string(msg.msg)) });
    }
};

struct asbridge_msg_server_report_valid_msg final {
    static constexpr auto description = "header must be SREPORT and msg must be OK, FAILED, or BROADCAST_FORWARD";

    vd::result operator()(const asbridge_msg& msg) const noexcept
    {
        if(msg.header == asbridge_msg_header::server_report
           && (msg.msg == asbridge_msg_type::ok || msg.msg == asbridge_msg_type::failed
               || msg.msg == asbridge_msg_type::broadcast_forward)) {
            return vd::result::ok();
        }

        return vd::result::failed(
            { std::format("{}. Invalid header/msg combination: {}/{}", description, to_string(msg.header), to_string(msg.msg)) });
    }
};

struct asbridge_msg_client_send_has_details final {
    static constexpr auto description = "details must be non-empty when header is CCOMMAND and msg is SEND";

    vd::result operator()(const asbridge_msg& msg) const noexcept
    {
        if(msg.header == asbridge_msg_header::client_command && msg.msg == asbridge_msg_type::send && msg.details.empty()) {
            return vd::result::failed(
                { std::format("{}. details must be non-empty when header is CCOMMAND and msg is SEND", description) });
        }

        return vd::result::ok();
    }
};

inline const auto asbridge_msg_full_check =
    vd::make_static_model<asbridge_msg>().with(asbridge_msg_header_known {}).with(asbridge_msg_msg_known {});
inline const auto asbridge_msg_failed_check = asbridge_msg_full_check.with(asbridge_msg_failed_details_non_empty {});

inline const auto asbridge_msg_client_command_send_check =
    asbridge_msg_full_check.with(asbridge_msg_client_command_send {}).with(asbridge_msg_client_send_has_details {});
inline const auto asbridge_msg_server_report_check = asbridge_msg_full_check.with(asbridge_msg_server_report_valid_msg {});
inline const auto asbridge_msg_broadcast_forward_check =
    asbridge_msg_server_report_check.with(asbridge_msg_broadcast_forward_details_non_empty {});
} // namespace as::proto::rules

namespace as::proto
{
std::expected<asbridge_msg, asbridge_msg_parse_error> parse_message(const std::string& raw_msg);
std::string serialize_message(const asbridge_msg& msg);
} // namespace as::proto
