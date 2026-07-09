#include <sstream>
#include <string_view>
#include <utility>

#include "proto/message.hxx"

namespace
{
using namespace as::proto;

constexpr std::string quoted(const std::string& str)
{
    return std::string("\"") + str + "\"";
}

constexpr auto invalid_header = static_cast<asbridge_msg_header>(0xFF);
constexpr auto invalid_msg_type = static_cast<asbridge_msg_type>(0xFF);

asbridge_msg_header header_from_string(std::string_view token) noexcept
{
    if(token == rules::message_header_client_command) {
        return asbridge_msg_header::client_command;
    }
    if(token == rules::message_header_server_report) {
        return asbridge_msg_header::server_report;
    }

    return invalid_header;
}

asbridge_msg_type msg_type_from_string(std::string_view token) noexcept
{
    if(token == rules::message_msg_send) {
        return asbridge_msg_type::send;
    }
    if(token == rules::message_msg_ok) {
        return asbridge_msg_type::ok;
    }
    if(token == rules::message_msg_failed) {
        return asbridge_msg_type::failed;
    }
    if(token == rules::message_msg_broadcast_forward) {
        return asbridge_msg_type::broadcast_forward;
    }
    if(token == rules::message_msg_service) {
        return asbridge_msg_type::service;
    }

    return invalid_msg_type;
}

// Splits the leading whitespace-delimited word off `s`. Returns {word, rest}; both are empty once `s` is exhausted.
std::pair<std::string_view, std::string_view> next_word(std::string_view s) noexcept
{
    const auto start = s.find_first_not_of(' ');
    if(start == std::string_view::npos) {
        return { {}, {} };
    }

    const auto end = s.find(' ', start);
    if(end == std::string_view::npos) {
        return { s.substr(start), {} };
    }

    return { s.substr(start, end - start), s.substr(end + 1) };
}

// Expects `s` to be a whitespace-separated sequence of double-quoted strings (no escaping). Returns false on malformed input.
bool parse_details(std::string_view s, std::vector<std::string>& out)
{
    std::size_t pos = 0;
    while(true) {
        while(pos < s.size() && s[pos] == ' ') {
            ++pos;
        }
        if(pos >= s.size()) {
            return true;
        }
        if(s[pos] != '"') {
            return false;
        }

        const auto close = s.find('"', pos + 1);
        if(close == std::string_view::npos) {
            return false;
        }

        out.emplace_back(s.substr(pos + 1, close - pos - 1));
        pos = close + 1;
    }
}

// Picks the validation model matching the parsed header/msg pair.
vd::result validate(const asbridge_msg& msg) noexcept
{
    if(msg.header == asbridge_msg_header::client_command) {
        return rules::asbridge_msg_client_command_send_check.check(msg);
    }

    if(msg.header == asbridge_msg_header::server_report) {
        switch(msg.msg) {
            case asbridge_msg_type::failed:
                return rules::asbridge_msg_failed_check.check(msg);
            case asbridge_msg_type::broadcast_forward:
                return rules::asbridge_msg_broadcast_forward_check.check(msg);
            case asbridge_msg_type::service:
                return rules::asbridge_msg_service_check.check(msg);
            default:
                return rules::asbridge_msg_server_report_check.check(msg);
        }
    }

    return rules::asbridge_msg_full_check.check(msg);
}
} // namespace

std::expected<as::proto::asbridge_msg, as::proto::asbridge_msg_parse_error> as::proto::parse_message(const std::string& raw_msg)
{
    const std::string_view view { raw_msg };

    const auto [header_token, after_header] = next_word(view);
    const auto [msg_token, after_msg] = next_word(after_header);

    if(header_token.empty() || msg_token.empty()) {
        return std::unexpected(asbridge_msg_parse_error { R"(Malformed message: expected "HEADER MSG [details...]")" });
    }

    asbridge_msg msg {
        .header = header_from_string(header_token),
        .msg = msg_type_from_string(msg_token),
        .details = {},
    };

    if(!parse_details(after_msg, msg.details)) {
        return std::unexpected(asbridge_msg_parse_error { "Malformed message: details must be double-quoted and whitespace-separated" });
    }

    if(auto validation = validate(msg); !validation.is_valid) {
        return std::unexpected(asbridge_msg_parse_error { validation.short_format() });
    }

    return msg;
}

std::string as::proto::serialize_message(const as::proto::asbridge_msg& msg)
{
    std::stringstream ss;

    ss << rules::to_string(msg.header) << ' ';
    ss << rules::to_string(msg.msg) << ' ';

    for(const auto& detail : msg.details) {
        ss << quoted(detail) << ' ';
    }

    return ss.str();
}
