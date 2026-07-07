#include <vd.hxx>

#include "htcore/system/duplex_pipe.hxx"

#include <chrono>
#include <thread>
#include <utility>

namespace
{
using steady_clock = std::chrono::steady_clock;

DWORD remaining_ms(steady_clock::time_point deadline) noexcept
{
    const auto now = steady_clock::now();
    if(now >= deadline) {
        return 0;
    }
    return static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
}

as::duplex_pipe::io_result map_last_error(DWORD error) noexcept
{
    switch(error) {
        case ERROR_MORE_DATA:
            return as::duplex_pipe::io_result::more_data;
        case ERROR_BROKEN_PIPE:
        case ERROR_PIPE_NOT_CONNECTED:
        case ERROR_NO_DATA:
            return as::duplex_pipe::io_result::disconnected;
        default:
            vd::require(false, "as::duplex_pipe: unexpected Win32 error {}", error);
            return as::duplex_pipe::io_result::disconnected; // unreachable, require() above aborts
    }
}

///@brief WriteFile/ReadFile on an overlapped handle can fail "synchronously" for outcomes that are
/// still routed correctly through GetOverlappedResult (ERROR_IO_PENDING, message-mode ERROR_MORE_DATA,
/// peer disconnects). Anything else means the call never reached the I/O manager at all (bad handle,
/// bad params) and the OVERLAPPED was never populated - that's a genuine contract violation.
void check_write_issue(BOOL issued) noexcept
{
    if(issued) {
        return;
    }
    const DWORD error = ::GetLastError();
    vd::require(error == ERROR_IO_PENDING || error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED || error == ERROR_NO_DATA,
        "as::duplex_pipe: WriteFile pre-flight failure, GetLastError={}",
        error);
}

void check_read_issue(BOOL issued) noexcept
{
    if(issued) {
        return;
    }
    const DWORD error = ::GetLastError();
    vd::require(error == ERROR_IO_PENDING || error == ERROR_MORE_DATA || error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED
                    || error == ERROR_NO_DATA,
        "as::duplex_pipe: ReadFile pre-flight failure, GetLastError={}",
        error);
}

void reap_completed_locked(std::vector<as::overlapped_ptr>& ops)
{
    std::erase_if(ops, [](const as::overlapped_ptr& op) {
        return op->is_complete();
    });
}

///@brief Cancels and drains every still-pending operation before the caller closes the handle.
/// Note: this relies on the operation having a chance to observe the peer's disconnect while it is
/// in flight (the OS wakes a pending overlapped ReadFile/WriteFile with an error once the peer goes
/// away). A read/write issued *after* the peer has already fully disconnected, with no operation
/// continuously in flight across that moment, can land in a state where CancelIoEx reports
/// ERROR_NOT_FOUND yet the operation never completes - a known sharp edge of named pipes, not
/// something this function can work around after the fact. Callers should not issue new operations
/// "cold" on a connection that may already be dead; wait_for_can_read() (which never blocks inside
/// the kernel's overlapped machinery) is safe to use for that check.
void drain_locked(std::vector<as::overlapped_ptr>& ops) noexcept
{
    for(auto& op : ops) {
        if(!op->is_complete()) {
            op->cancel();
            std::size_t transferred = 0;
            op->get_result(transferred, true);
        }
    }
    ops.clear();
}
} // namespace

namespace ht
{
duplex_pipe::duplex_pipe(as::file_handle handle, as::system_path fname, bool is_server, std::size_t buffer_size) noexcept
    : m_fname(fname), m_file(handle), m_readwrite_length(buffer_size), m_is_server(is_server),
      m_connected(handle != as::invalid_handle && !is_server), m_pending(std::make_unique<pending_ops>())
{
}

duplex_pipe::duplex_pipe(as::file_handle handle, as::system_path fname) : duplex_pipe(handle, fname, false, default_buffer_size)
{
}

duplex_pipe::~duplex_pipe()
{
    teardown();
}

duplex_pipe::duplex_pipe(duplex_pipe&& other) noexcept
    : m_fname(other.m_fname), m_file(other.m_file), m_readwrite_length(other.m_readwrite_length), m_flags(other.m_flags),
      m_is_server(other.m_is_server), m_connected(other.m_connected), m_pending(std::move(other.m_pending))
{
    other.m_file = as::invalid_handle;
    other.m_connected = false;
}

duplex_pipe& duplex_pipe::operator=(duplex_pipe&& other)
{
    if(this == &other) {
        return *this;
    }

    vd::require(!connected(), "as::duplex_pipe::operator=: overwriting an object with a live connection; tear it down explicitly first");

    teardown();

    m_fname = other.m_fname;
    m_file = other.m_file;
    m_readwrite_length = other.m_readwrite_length;
    m_flags = other.m_flags;
    m_is_server = other.m_is_server;
    m_connected = other.m_connected;
    m_pending = std::move(other.m_pending);

    other.m_file = as::invalid_handle;
    other.m_connected = false;

    return *this;
}

void duplex_pipe::teardown() noexcept
{
    if(!valid()) {
        return;
    }

    if(m_pending) {
        std::scoped_lock lock(m_pending->mtx);
        drain_locked(m_pending->ops);
    }

    // Block until the peer has actually read everything this side already wrote. Without this,
    // DisconnectNamedPipe/CloseHandle can tear the pipe down while data this side wrote (including
    // an application-level "stop" message) is still sitting unread in the kernel buffer - that data
    // is then silently lost, and the peer's next read lands strictly after the disconnect with no
    // operation in flight across it, which can hang forever (see win32_overlapped/drain_locked notes).
    // Trade-off: if the peer stops reading entirely (e.g. it crashed), this blocks teardown() until
    // the peer disconnects on its own; that is judged preferable to the silent data loss + hang above.
    if(m_connected) {
        ::FlushFileBuffers(m_file);
    }

    if(m_is_server) {
        ::DisconnectNamedPipe(m_file);
    }
    ::CloseHandle(m_file);
    m_file = as::invalid_handle;
    m_connected = false;
}

bool duplex_pipe::valid() const noexcept
{
    return m_file != as::invalid_handle;
}

bool duplex_pipe::connected() const noexcept
{
    return m_connected;
}

bool duplex_pipe::is_server() const noexcept
{
    return m_is_server;
}

bool duplex_pipe::accept(int timeout)
{
    vd::require(valid(), "as::duplex_pipe::accept: pipe handle is invalid");
    vd::require(m_is_server, "as::duplex_pipe::accept: called on a client-side pipe");
    vd::require(!m_connected, "as::duplex_pipe::accept: pipe is already connected");

    as::win32_overlapped ov(m_file);

    if(::ConnectNamedPipe(m_file, ov.native())) {
        m_connected = true;
        return true;
    }

    const DWORD issue_error = ::GetLastError();
    if(issue_error == ERROR_PIPE_CONNECTED) {
        m_connected = true;
        return true;
    }
    vd::require(issue_error == ERROR_IO_PENDING, "as::duplex_pipe::accept: ConnectNamedPipe failed, GetLastError={}", issue_error);

    if(!ov.wait(timeout)) {
        ov.cancel();
        std::size_t transferred = 0;
        ov.get_result(transferred, true);
        return false;
    }

    std::size_t transferred = 0;
    m_connected = ov.get_result(transferred, false);
    return m_connected;
}

duplex_pipe::io_result duplex_pipe::put(std::span<const std::byte> content, std::size_t length, std::size_t& bytes_written)
{
    vd::require(valid(), "as::duplex_pipe::put: pipe handle is invalid");
    vd::required(length <= content.size(), "as::duplex_pipe::put: length {} exceeds content size {}", length, content.size());

    as::win32_overlapped ov(m_file);

    check_write_issue(::WriteFile(m_file, content.data(), static_cast<DWORD>(length), nullptr, ov.native()));

    const bool completed = ov.get_result(bytes_written, true);
    return completed ? io_result::ok : map_last_error(ov.last_error());
}

duplex_pipe::io_result duplex_pipe::put(const std::vector<std::byte>& content, std::size_t& bytes_written)
{
    return put(std::span(content), content.size(), bytes_written);
}

duplex_pipe::io_result duplex_pipe::read_to(std::span<std::byte> buffer, std::size_t length, std::size_t& bytes_read)
{
    vd::require(valid(), "as::duplex_pipe::read_to: pipe handle is invalid");
    vd::required(length <= buffer.size(), "as::duplex_pipe::read_to: length {} exceeds buffer size {}", length, buffer.size());

    as::win32_overlapped ov(m_file);

    check_read_issue(::ReadFile(m_file, buffer.data(), static_cast<DWORD>(length), nullptr, ov.native()));

    const bool completed = ov.get_result(bytes_read, true);
    return completed ? io_result::ok : map_last_error(ov.last_error());
}

duplex_pipe::io_result duplex_pipe::read_to(std::vector<std::byte>& buffer, std::size_t length, std::size_t& bytes_read)
{
    return read_to(std::span(buffer), length, bytes_read);
}

std::vector<std::byte> duplex_pipe::read_all()
{
    std::vector<std::byte> result(m_readwrite_length);
    std::size_t total = 0;

    for(;;) {
        std::size_t got = 0;
        const io_result result_code = read_to(std::span(result).subspan(total), result.size() - total, got);
        total += got;

        if(result_code != io_result::more_data) {
            break;
        }
        result.resize(result.size() * 2);
    }

    result.resize(total);
    return result;
}

as::overlapped_ptr duplex_pipe::put_overlapped(std::span<const std::byte> content, std::size_t length)
{
    vd::require(valid(), "as::duplex_pipe::put_overlapped: pipe handle is invalid");
    vd::required(length <= content.size(), "as::duplex_pipe::put_overlapped: length {} exceeds content size {}", length, content.size());

    auto op = std::make_shared<as::win32_overlapped>(m_file);
    {
        std::scoped_lock lock(m_pending->mtx);
        reap_completed_locked(m_pending->ops);
        m_pending->ops.push_back(op);
    }

    check_write_issue(::WriteFile(m_file, content.data(), static_cast<DWORD>(length), nullptr, op->native()));
    return op;
}

as::overlapped_ptr duplex_pipe::put_overlapped(const std::vector<std::byte>& content)
{
    return put_overlapped(std::span(content), content.size());
}

as::overlapped_ptr duplex_pipe::read_to_overlapped(std::span<std::byte> buffer, std::size_t length)
{
    vd::require(valid(), "as::duplex_pipe::read_to_overlapped: pipe handle is invalid");
    vd::required(length <= buffer.size(), "as::duplex_pipe::read_to_overlapped: length {} exceeds buffer size {}", length, buffer.size());

    auto op = std::make_shared<as::win32_overlapped>(m_file);
    {
        std::scoped_lock lock(m_pending->mtx);
        reap_completed_locked(m_pending->ops);
        m_pending->ops.push_back(op);
    }

    check_read_issue(::ReadFile(m_file, buffer.data(), static_cast<DWORD>(length), nullptr, op->native()));
    return op;
}

as::overlapped_ptr duplex_pipe::read_to_overlapped(std::vector<std::byte>& buffer)
{
    return read_to_overlapped(std::span(buffer), buffer.size());
}

duplex_pipe::io_result duplex_pipe::overlapped_result(const as::overlapped_ptr& op, std::size_t& bytes_transferred, bool wait)
{
    if(op->get_result(bytes_transferred, wait)) {
        return io_result::ok;
    }

    const DWORD error = op->last_error();
    if(error == ERROR_IO_INCOMPLETE) {
        return io_result::pending;
    }
    return map_last_error(error);
}

bool duplex_pipe::wait_for_can_read(int timeout)
{
    constexpr auto poll_interval = std::chrono::milliseconds(1);

    const bool has_deadline = timeout >= 0;
    const steady_clock::time_point deadline =
        has_deadline ? steady_clock::now() + std::chrono::milliseconds(timeout) : steady_clock::time_point {};

    for(;;) {
        DWORD available = 0;
        if(!::PeekNamedPipe(m_file, nullptr, 0, nullptr, &available, nullptr)) {
            return false;
        }
        if(available > 0) {
            return true;
        }
        if(has_deadline && steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(poll_interval);
    }
}
} // namespace ht

namespace ht
{
duplex_pipe create_duplex_pipe(as::system_path name, std::size_t buffer_size)
{
    const HANDLE handle = ::CreateNamedPipeW(name,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        static_cast<DWORD>(buffer_size),
        static_cast<DWORD>(buffer_size),
        0,
        nullptr);

    return duplex_pipe(handle, name, true, buffer_size);
}

duplex_pipe open_duplex_pipe(as::system_path name, int timeout)
{
    const bool has_deadline = timeout >= 0;
    const steady_clock::time_point deadline =
        has_deadline ? steady_clock::now() + std::chrono::milliseconds(timeout) : steady_clock::time_point {};

    for(;;) {
        const HANDLE handle = ::CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        if(handle != INVALID_HANDLE_VALUE) {
            DWORD mode = PIPE_READMODE_MESSAGE;
            ::SetNamedPipeHandleState(handle, &mode, nullptr, nullptr);
            return duplex_pipe(handle, name, false, duplex_pipe::default_buffer_size);
        }

        if(::GetLastError() != ERROR_PIPE_BUSY) {
            return duplex_pipe(as::invalid_handle, name, false, 0);
        }
        if(has_deadline && steady_clock::now() >= deadline) {
            return duplex_pipe(as::invalid_handle, name, false, 0);
        }

        const DWORD wait_ms = has_deadline ? remaining_ms(deadline) : NMPWAIT_WAIT_FOREVER;
        if(!::WaitNamedPipeW(name, wait_ms)) {
            return duplex_pipe(as::invalid_handle, name, false, 0);
        }
    }
}
} // namespace ht
