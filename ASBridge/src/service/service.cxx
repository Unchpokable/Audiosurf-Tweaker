#include "service/service.hxx"

#include "pipes/duplex_pipe.hxx"
#include "pipes/win32_overlapped.hxx"

#include "proto/message.hxx"

#include "system/string.hxx"

#include "window/native_window.hxx"
#include "window/wnd_handle.hxx"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr auto k_audiosurf_window_title = L"QuestViewer.exe";
constexpr auto k_listener_registration_prefix = L"asregisterlistenerwindow ";

constexpr UINT_PTR k_liveness_timer_id = 1;
constexpr UINT k_liveness_timer_interval_ms = 30;

constexpr std::size_t k_pipe_buffer_size = 4096;
constexpr int k_pipe_poll_ms = 30;
} // namespace

namespace
{
// Touched only by the window/timer thread (main thread).
as::wnd::wnd_handle audiosurf_window;
bool audiosurf_last_known_valid = false;
as::sys_string wnd_title_storage;

// Published snapshot of audiosurf_window's HWND for pipe_pump_thread to read without touching wnd_handle.
std::atomic<HWND> cached_audiosurf_hwnd { nullptr };
} // namespace

namespace
{
// Serialized SREPORT lines waiting to go out over the pipe. Pushed by the window thread
// (BROADCAST_FORWARD / SERVICE) and by pipe_pump_thread itself (OK / FAILED acks); drained only by
// pipe_pump_thread, which is the sole writer of pipe_channel.
std::queue<std::string> reports_to_send;
std::mutex reports_mutex;
std::condition_variable reports_condition;
} // namespace

namespace
{
std::optional<as::duplex_pipe> pipe_channel;
as::sys_string pipe_name_storage;
std::jthread pipe_pump_thread;
} // namespace

namespace
{
std::string to_narrow(const wchar_t* wide, int wide_len)
{
    if(wide_len <= 0) {
        return {};
    }

    int narrow_len = WideCharToMultiByte(CP_UTF8, 0, wide, wide_len, nullptr, 0, nullptr, nullptr);
    if(narrow_len <= 0) {
        return {};
    }

    std::string narrow(static_cast<std::size_t>(narrow_len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, wide_len, narrow.data(), narrow_len, nullptr, nullptr);
    return narrow;
}

bool send_wide_copydata(HWND target, const std::wstring& text)
{
    COPYDATASTRUCT cds {};
    cds.dwData = 0;
    cds.cbData = static_cast<DWORD>(text.size() * sizeof(wchar_t));
    cds.lpData = const_cast<wchar_t*>(text.data()); // NOLINT COPYDATASTRUCT::lpData is not const in the Win32 API

    return ::SendMessageW(
               target, WM_COPYDATA, reinterpret_cast<WPARAM>(as::wnd::get_window().native()), reinterpret_cast<LPARAM>(&cds))
        != 0;
}

void send_listener_registration(HWND target)
{
    send_wide_copydata(target, k_listener_registration_prefix + wnd_title_storage);
}

void push_report(const as::proto::asbridge_msg& msg)
{
    {
        std::scoped_lock lock(reports_mutex);
        reports_to_send.push(as::proto::serialize_message(msg));
    }
    reports_condition.notify_one();
}

LRESULT process_wm_copydata(HWND, WPARAM, LPARAM lparam)
{
    auto copydata = reinterpret_cast<COPYDATASTRUCT*>(lparam); // NOLINT System API

    if(copydata && copydata->lpData != nullptr && copydata->cbData > 0) {
        auto raw_bytes = static_cast<const char*>(copydata->lpData);
        auto raw_len = static_cast<int>(copydata->cbData);

        int unicode_flags = IS_TEXT_UNICODE_UNICODE_MASK;
        bool looks_unicode = IsTextUnicode(raw_bytes, raw_len, &unicode_flags) != FALSE;

        std::string data = looks_unicode
                               ? to_narrow(reinterpret_cast<const wchar_t*>(raw_bytes), raw_len / static_cast<int>(sizeof(wchar_t)))
                               : std::string(raw_bytes, static_cast<std::size_t>(raw_len));

        push_report({
            .header = as::proto::asbridge_msg_header::server_report,
            .msg = as::proto::asbridge_msg_type::broadcast_forward,
            .details = { std::move(data) },
        });
    }

    return TRUE;
}

LRESULT process_wm_timer(HWND, WPARAM wparam, LPARAM)
{
    if(wparam != k_liveness_timer_id) {
        return 0;
    }

    bool currently_valid = audiosurf_window.valid();
    if(!currently_valid) {
        audiosurf_window = as::wnd::wnd_handle::open_existing(k_audiosurf_window_title);
        currently_valid = audiosurf_window.valid();
    }

    cached_audiosurf_hwnd.store(currently_valid ? audiosurf_window.native() : nullptr, std::memory_order_relaxed);

    if(currently_valid != audiosurf_last_known_valid) {
        audiosurf_last_known_valid = currently_valid;

        if(currently_valid) {
            send_listener_registration(audiosurf_window.native());
        }

        push_report({
            .header = as::proto::asbridge_msg_header::server_report,
            .msg = as::proto::asbridge_msg_type::service,
            .details = { currently_valid ? as::proto::rules::service_status_window_found
                                          : as::proto::rules::service_status_window_lost },
        });
    }

    return 0;
}
} // namespace

namespace
{
void reset_pipe()
{
    // pipe_channel.operator= refuses to overwrite a "connected" instance (see duplex_pipe's guard);
    // destroying it explicitly runs teardown() unconditionally, which is what we want once the peer
    // is known to be gone.
    pipe_channel.reset();
    pipe_channel.emplace(as::create_duplex_pipe(pipe_name_storage.c_str(), k_pipe_buffer_size));
}

bool send_to_audiosurf(HWND target, const std::string& detail)
{
    int wide_len = ::MultiByteToWideChar(CP_UTF8, 0, detail.data(), static_cast<int>(detail.size()), nullptr, 0);
    if(wide_len <= 0 && !detail.empty()) {
        return false;
    }

    std::wstring wide(static_cast<std::size_t>(wide_len), L'\0');
    if(wide_len > 0) {
        ::MultiByteToWideChar(CP_UTF8, 0, detail.data(), static_cast<int>(detail.size()), wide.data(), wide_len);
    }

    return send_wide_copydata(target, wide);
}

void handle_client_command(const as::proto::asbridge_msg& msg)
{
    if(msg.header != as::proto::asbridge_msg_header::client_command || msg.msg != as::proto::asbridge_msg_type::send) {
        return;
    }

    HWND target = cached_audiosurf_hwnd.load(std::memory_order_relaxed);
    bool delivered = target != nullptr && send_to_audiosurf(target, msg.details.front());

    push_report({
        .header = as::proto::asbridge_msg_header::server_report,
        .msg = delivered ? as::proto::asbridge_msg_type::ok : as::proto::asbridge_msg_type::failed,
        .details = delivered ? std::vector<std::string> {}
                              : std::vector<std::string> { "audiosurf window unavailable or SendMessage failed" },
    });
}

void drain_reports_to_pipe(std::unique_lock<std::mutex>& lock)
{
    while(!reports_to_send.empty()) {
        std::string next = std::move(reports_to_send.front());
        reports_to_send.pop();
        lock.unlock();

        std::vector<std::byte> bytes(
            reinterpret_cast<const std::byte*>(next.data()), reinterpret_cast<const std::byte*>(next.data() + next.size()));

        std::size_t written = 0;
        auto result = pipe_channel->put(bytes, written);

        lock.lock();

        if(result == as::duplex_pipe::io_result::disconnected) {
            reset_pipe();
            return;
        }
    }
}

void pipe_pump_loop(std::stop_token stop_token)
{
    as::overlapped_ptr pending_read;
    std::vector<std::byte> read_buffer(k_pipe_buffer_size);

    while(!stop_token.stop_requested()) {
        if(!pipe_channel->connected()) {
            pipe_channel->accept(k_pipe_poll_ms);
            continue;
        }

        if(!pending_read) {
            pending_read = pipe_channel->read_to_overlapped(read_buffer);
        }

        std::size_t transferred = 0;
        auto result = pipe_channel->overlapped_result(pending_read, transferred, false);

        if(result == as::duplex_pipe::io_result::disconnected) {
            pending_read.reset();
            reset_pipe();
        } else if(result != as::duplex_pipe::io_result::pending) {
            if(result == as::duplex_pipe::io_result::ok) {
                std::string raw(reinterpret_cast<const char*>(read_buffer.data()), transferred);
                if(auto parsed = as::proto::parse_message(raw); parsed.has_value()) {
                    handle_client_command(*parsed);
                }
                // malformed input (parse error) is silently dropped - nothing meaningful to ack.
            }
            // over-sized message (more_data) is dropped the same way - protocol lines are expected to be short.
            pending_read.reset();
        }

        std::unique_lock lock(reports_mutex);
        if(reports_to_send.empty()) {
            reports_condition.wait_for(lock, std::chrono::milliseconds(k_pipe_poll_ms), [] {
                return !reports_to_send.empty();
            });
        }
        drain_reports_to_pipe(lock);
    }
}
} // namespace

namespace as::liveipc
{
void initialize(as::raw_sys_const_string wnd_title, as::raw_sys_const_string pipe_name)
{
    wnd_title_storage = wnd_title;

    as::wnd::initialize(wnd_title);
    as::wnd::set_handler_for(WM_COPYDATA, process_wm_copydata);
    as::wnd::set_handler_for(WM_TIMER, process_wm_timer);

    pipe_name_storage = pipe_name;
    pipe_channel.emplace(as::create_duplex_pipe(pipe_name_storage.c_str(), k_pipe_buffer_size));

    ::SetTimer(as::wnd::get_window().native(), k_liveness_timer_id, k_liveness_timer_interval_ms, nullptr);

    pipe_pump_thread = std::jthread(pipe_pump_loop);
}

void shutdown()
{
    if(pipe_pump_thread.joinable()) {
        pipe_pump_thread.request_stop();
        reports_condition.notify_all();
        pipe_pump_thread.join();
    }

    ::KillTimer(as::wnd::get_window().native(), k_liveness_timer_id);

    pipe_channel.reset();

    as::wnd::shutdown();
}
} // namespace as::liveipc
