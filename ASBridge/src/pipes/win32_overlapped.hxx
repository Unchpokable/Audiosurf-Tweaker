#pragma once

#ifndef HT_WIN32_OVERLAPPED_HXX
#define HT_WIN32_OVERLAPPED_HXX

#if defined(_WIN32)

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstddef>
#include <memory>

namespace as
{
///@brief RAII owner of a single in-flight (or not-yet-issued) Win32 overlapped I/O operation: the
/// OVERLAPPED block itself, its dedicated completion event, and the file handle it was issued
/// against. Construct one, pass native() into ReadFile/WriteFile/ConnectNamedPipe etc., then use
/// is_complete()/wait()/get_result() instead of calling the raw WinAPI by hand.
///
/// If destroyed while still in flight, cancels and drains the operation first so the OVERLAPPED
/// memory is never freed while the kernel might still be writing into it.
class win32_overlapped final {
public:
    static constexpr int wait_infinite = -1;

    explicit win32_overlapped(HANDLE file);
    ~win32_overlapped();

    win32_overlapped(const win32_overlapped&) = delete;
    win32_overlapped& operator=(const win32_overlapped&) = delete;
    win32_overlapped(win32_overlapped&&) = delete;
    win32_overlapped& operator=(win32_overlapped&&) = delete;

    ///@brief The OVERLAPPED block to pass into the WinAPI call that starts the operation.
    OVERLAPPED* native() noexcept;

    HANDLE event() const noexcept;
    HANDLE file() const noexcept;

    ///@brief Non-blocking check; does not retrieve the result.
    bool is_complete() const noexcept;

    ///@brief Waits for completion without retrieving the result. timeout_ms < 0 waits indefinitely.
    /// Returns false on timeout, true if the operation completed (successfully or not).
    bool wait(int timeout_ms = wait_infinite) const noexcept;

    ///@brief Retrieves the outcome (mirrors GetOverlappedResult). wait_for_completion=true blocks
    /// until done; false polls once. Returns true on success with bytes_transferred set; on false,
    /// last_error() holds the Win32 error (ERROR_IO_INCOMPLETE if simply not done yet).
    bool get_result(std::size_t& bytes_transferred, bool wait_for_completion = false) noexcept;

    ///@brief Requests cancellation on its file handle. Does not wait - follow with wait()/
    /// get_result(wait_for_completion=true) to observe the outcome. A return of false with
    /// last Win32 error ERROR_NOT_FOUND means the operation already completed or was never pending;
    /// it does not by itself mean the operation is stuck.
    bool cancel() noexcept;

    DWORD last_error() const noexcept;
    std::size_t bytes_transferred() const noexcept;

private:
    HANDLE m_file;
    OVERLAPPED m_overlapped {};
    std::size_t m_bytes_transferred { 0 };
    DWORD m_last_error { 0 };
};

using overlapped_ptr = std::shared_ptr<win32_overlapped>;
} // namespace as

#else
#error "win32_overlapped is Windows-only"
#endif

#endif
