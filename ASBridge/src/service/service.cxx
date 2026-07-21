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
constexpr auto k_audiosurf_process_exe = L"QuestViewer.exe";

// The game protocol distinguishes a freshly started game: within this window after process start
// the legacy tweaker sent "quickstartregisterwindow" instead of "registerlistenerwindow".
constexpr double k_quickstart_process_age_seconds = 30.0;

constexpr UINT_PTR k_liveness_timer_id = 1;
constexpr UINT k_liveness_timer_interval_ms = 30;

constexpr std::size_t k_pipe_buffer_size = 4096;
constexpr int k_pipe_poll_ms = 30;

constexpr std::string_view k_tw_ovl_prefix = "TW_OVL ";
} // namespace

namespace
{
// Touched only by the window/timer thread (main thread).
as::wnd::wnd_handle audiosurf_window;
DWORD audiosurf_pid = 0;
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

// The game expects single-byte ANSI text: the legacy tweaker marshalled COPYDATASTRUCT::lpData as
// UnmanagedType.LPStr with cbData = length + 1 (null terminator included). Wide strings are not
// understood by the game side, so everything outbound goes through this narrow variant.
bool send_ansi_copydata(HWND target, const std::string& text)
{
    COPYDATASTRUCT cds {};
    cds.dwData = 0;
    cds.cbData = static_cast<DWORD>(text.size() + 1);
    cds.lpData = const_cast<char*>(text.c_str()); // NOLINT COPYDATASTRUCT::lpData is not const in the Win32 API

    return ::SendMessageW(
               target, WM_COPYDATA, reinterpret_cast<WPARAM>(as::wnd::get_window().native()), reinterpret_cast<LPARAM>(&cds))
        != 0;
}

// Age of a process in seconds, or a huge value when it cannot be determined - mirroring the legacy
// tweaker, which fell back to "not a quickstart" when process times were inaccessible.
double process_age_seconds(DWORD pid)
{
    constexpr double k_unknown_age = 1.0e9;

    as::proc_handle process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if(process == nullptr) {
        return k_unknown_age;
    }

    FILETIME creation {}, exit {}, kernel {}, user {};
    const BOOL got_times = ::GetProcessTimes(process, &creation, &exit, &kernel, &user);
    ::CloseHandle(process);

    if(got_times == FALSE) {
        return k_unknown_age;
    }

    FILETIME now {};
    ::GetSystemTimeAsFileTime(&now);

    ULARGE_INTEGER created {};
    created.LowPart = creation.dwLowDateTime;
    created.HighPart = creation.dwHighDateTime;

    ULARGE_INTEGER current {};
    current.LowPart = now.dwLowDateTime;
    current.HighPart = now.dwHighDateTime;

    if(current.QuadPart <= created.QuadPart) {
        return 0.0;
    }

    constexpr double k_filetime_ticks_per_second = 1.0e7; // FILETIME is in 100ns units
    return static_cast<double>(current.QuadPart - created.QuadPart) / k_filetime_ticks_per_second;
}

void send_listener_registration(HWND target, DWORD pid)
{
    // Legacy protocol verbatim: a freshly started game gets "quickstartregisterwindow", an already
    // running one gets "registerlistenerwindow"; the game acks with "successfullyquickstartregistered"
    // / "successfullyregistered" broadcast back to the listener window named here.
    const bool quickstart = process_age_seconds(pid) < k_quickstart_process_age_seconds;

    std::string command = std::string("ascommand ") + (quickstart ? "quickstartregisterwindow " : "registerlistenerwindow ")
        + to_narrow(wnd_title_storage.data(), static_cast<int>(wnd_title_storage.size()));

    send_ansi_copydata(target, command);
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

        // cbData conventionally includes the null terminator; don't leak it into the protocol.
        while(!data.empty() && data.back() == '\0') {
            data.pop_back();
        }

        const bool is_tw_ovl = data.size() >= k_tw_ovl_prefix.size()
            && std::equal(k_tw_ovl_prefix.begin(), k_tw_ovl_prefix.end(), data.begin());

        if(is_tw_ovl) {
            const auto inner = data.substr(k_tw_ovl_prefix.size());
            push_report({
                .header = as::proto::asbridge_msg_header::server_report,
                .msg = as::proto::asbridge_msg_type::overlay_forward,
                .details = { std::string(inner) },
            });
        } else {
            push_report({
                .header = as::proto::asbridge_msg_header::server_report,
                .msg = as::proto::asbridge_msg_type::broadcast_forward,
                .details = { std::move(data) },
            });
        }
    }

    return TRUE;
}

struct found_game_window final {
    HWND hwnd = nullptr;
    DWORD pid = 0;
};

// EnumWindows callback state for find_audiosurf_main_window().
struct enum_windows_state final {
    found_game_window result;
};

bool process_exe_matches(DWORD pid, const wchar_t* expected_exe)
{
    as::proc_handle process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if(process == nullptr) {
        return false;
    }

    wchar_t image_path[MAX_PATH] = {};
    DWORD path_len = MAX_PATH;
    const BOOL queried = ::QueryFullProcessImageNameW(process, 0, image_path, &path_len);
    ::CloseHandle(process);

    if(queried == FALSE) {
        return false;
    }

    const wchar_t* basename = ::wcsrchr(image_path, L'\\');
    basename = basename != nullptr ? basename + 1 : image_path;

    return ::_wcsicmp(basename, expected_exe) == 0;
}

BOOL CALLBACK enum_windows_proc(HWND hwnd, LPARAM lparam)
{
    auto* state = reinterpret_cast<enum_windows_state*>(lparam); // NOLINT System API

    // Emulates .NET's Process.MainWindowHandle pick, which is what the legacy tweaker relied on:
    // a visible, unowned top-level window belonging to the target process.
    if(::IsWindowVisible(hwnd) == FALSE || ::GetWindow(hwnd, GW_OWNER) != nullptr) {
        return TRUE;
    }

    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);

    if(pid == 0 || !process_exe_matches(pid, k_audiosurf_process_exe)) {
        return TRUE;
    }

    state->result = { .hwnd = hwnd, .pid = pid };
    return FALSE; // found - stop enumerating
}

// The game window is located by process exe name, not by window title: the title of the game's
// main window is not guaranteed to be anything in particular, while the process is always
// QuestViewer.exe (same lookup the legacy tweaker did via Process.GetProcessesByName).
found_game_window find_audiosurf_main_window()
{
    enum_windows_state state;
    ::EnumWindows(enum_windows_proc, reinterpret_cast<LPARAM>(&state)); // NOLINT System API
    return state.result;
}

LRESULT process_wm_timer(HWND, WPARAM wparam, LPARAM)
{
    if(wparam != k_liveness_timer_id) {
        return 0;
    }

    bool currently_valid = audiosurf_window.valid();
    if(!currently_valid) {
        auto found = find_audiosurf_main_window();
        if(found.hwnd != nullptr) {
            audiosurf_window = as::wnd::wnd_handle(found.hwnd);
            audiosurf_pid = found.pid;
        }
        currently_valid = audiosurf_window.valid();
    }

    cached_audiosurf_hwnd.store(currently_valid ? audiosurf_window.native() : nullptr, std::memory_order_relaxed);

    if(currently_valid != audiosurf_last_known_valid) {
        audiosurf_last_known_valid = currently_valid;

        // WINDOW_FOUND must be queued before the registration goes out: the game replies to the
        // registration synchronously (SendMessage re-enters process_wm_copydata before
        // send_listener_registration returns), and the client expects "window found" to precede
        // the game's "successfullyregistered" broadcast.
        push_report({
            .header = as::proto::asbridge_msg_header::server_report,
            .msg = as::proto::asbridge_msg_type::service,
            .details = currently_valid
                ? std::vector<std::string> { as::proto::rules::service_status_window_found, std::to_string(audiosurf_pid) }
                : std::vector<std::string> { as::proto::rules::service_status_window_lost },
        });

        if(currently_valid) {
            send_listener_registration(audiosurf_window.native(), audiosurf_pid);
        }
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
    // Game commands are plain ASCII, and the game reads lpData as a single-byte string - the
    // pipe-side UTF-8 payload can go out as-is (see send_ansi_copydata for the protocol rationale).
    return send_ansi_copydata(target, detail);
}

void handle_client_command(const as::proto::asbridge_msg& msg)
{
    if(msg.header != as::proto::asbridge_msg_header::client_command) {
        return;
    }

    HWND target = cached_audiosurf_hwnd.load(std::memory_order_relaxed);
    if(target == nullptr) {
        // still ack with FAILED so the managed side does not block indefinitely on an assumed SendMessage delivery
        if(msg.msg == as::proto::asbridge_msg_type::send) {
            push_report({
                .header = as::proto::asbridge_msg_header::server_report,
                .msg = as::proto::asbridge_msg_type::failed,
                .details = { "audiosurf window unavailable or SendMessage failed" },
            });
        } else if(msg.msg == as::proto::asbridge_msg_type::overlay_send) {
            push_report({
                .header = as::proto::asbridge_msg_header::server_report,
                .msg = as::proto::asbridge_msg_type::overlay_failed,
                .details = { "audiosurf window unavailable or SendMessage failed" },
            });
        }
        return;
    }

    if(msg.msg == as::proto::asbridge_msg_type::send) {
        bool delivered = send_to_audiosurf(target, msg.details.front());
        push_report({
            .header = as::proto::asbridge_msg_header::server_report,
            .msg = delivered ? as::proto::asbridge_msg_type::ok : as::proto::asbridge_msg_type::failed,
            .details = delivered ? std::vector<std::string> {}
                                  : std::vector<std::string> { "audiosurf window unavailable or SendMessage failed" },
        });
        return;
    }

    if(msg.msg == as::proto::asbridge_msg_type::overlay_send) {
        const auto& payload = msg.details.front();
        std::string envelope;
        envelope.reserve(k_tw_ovl_prefix.size() + payload.size());
        envelope.append(k_tw_ovl_prefix.begin(), k_tw_ovl_prefix.end());
        envelope.append(payload);

        bool delivered = send_to_audiosurf(target, envelope);
        push_report({
            .header = as::proto::asbridge_msg_header::server_report,
            .msg = delivered ? as::proto::asbridge_msg_type::overlay_ok : as::proto::asbridge_msg_type::overlay_failed,
            .details = delivered ? std::vector<std::string> {}
                                  : std::vector<std::string> { "audiosurf window unavailable or SendMessage failed" },
        });
    }
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
