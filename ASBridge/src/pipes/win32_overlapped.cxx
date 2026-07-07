#include <vd.hxx>

#include "htcore/system/win32_overlapped.hxx"

namespace ht
{
win32_overlapped::win32_overlapped(HANDLE file) : m_file(file)
{
    m_overlapped.hEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    vd::require(m_overlapped.hEvent != nullptr, "ht::win32_overlapped: CreateEventW failed, GetLastError={}", ::GetLastError());
}

win32_overlapped::~win32_overlapped()
{
    if(!is_complete()) {
        ::CancelIoEx(m_file, &m_overlapped);
        DWORD transferred = 0;
        ::GetOverlappedResult(m_file, &m_overlapped, &transferred, TRUE);
    }
    ::CloseHandle(m_overlapped.hEvent);
}

OVERLAPPED* win32_overlapped::native() noexcept
{
    return &m_overlapped;
}

HANDLE win32_overlapped::event() const noexcept
{
    return m_overlapped.hEvent;
}

HANDLE win32_overlapped::file() const noexcept
{
    return m_file;
}

bool win32_overlapped::is_complete() const noexcept
{
    return HasOverlappedIoCompleted(&m_overlapped) != 0;
}

bool win32_overlapped::wait(int timeout_ms) const noexcept
{
    const DWORD ms = timeout_ms < 0 ? INFINITE : static_cast<DWORD>(timeout_ms);
    return ::WaitForSingleObject(m_overlapped.hEvent, ms) == WAIT_OBJECT_0;
}

bool win32_overlapped::get_result(std::size_t& bytes_transferred, bool wait_for_completion) noexcept
{
    DWORD transferred = 0;
    const BOOL ok = ::GetOverlappedResult(m_file, &m_overlapped, &transferred, wait_for_completion ? TRUE : FALSE);
    m_bytes_transferred = transferred;
    bytes_transferred = transferred;
    m_last_error = ok ? ERROR_SUCCESS : ::GetLastError();
    return ok != 0;
}

bool win32_overlapped::cancel() noexcept
{
    return ::CancelIoEx(m_file, &m_overlapped) != 0;
}

DWORD win32_overlapped::last_error() const noexcept
{
    return m_last_error;
}

std::size_t win32_overlapped::bytes_transferred() const noexcept
{
    return m_bytes_transferred;
}
} // namespace ht
