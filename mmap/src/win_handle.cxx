#include "global.h"

#include "win_handle.hxx"

mmap::raii::WinHandle::WinHandle(HANDLE handle) noexcept
    : _handle(handle)
    , _was_moved(false)
    , _was_released(false)
{
}

mmap::raii::WinHandle::WinHandle() noexcept
    : _handle(INVALID_HANDLE_VALUE)
    , _was_moved(false)
    , _was_released(false)
{
}

mmap::raii::WinHandle::~WinHandle() noexcept
{
    if(!_was_released && !_was_moved && _handle != INVALID_HANDLE_VALUE) {
        CloseHandle(_handle);
    }
}

mmap::raii::WinHandle::WinHandle(WinHandle&& other) noexcept
    : _handle(other._handle)
    , _was_moved(false)
    , _was_released(false)
{
    other._was_moved = true;
}

mmap::raii::WinHandle& mmap::raii::WinHandle::operator=(WinHandle&& other) noexcept
{
    if(this != &other) {
        // Release current handle if valid
        if(!_was_released && _handle != INVALID_HANDLE_VALUE) {
            CloseHandle(_handle);
        }

        // Move from other
        _handle = other._handle;
        _was_moved = false;
        _was_released = false;

        other._was_moved = true;
    }

    return *this;
}

void mmap::raii::WinHandle::release() noexcept
{
    if(!_was_released && _handle != INVALID_HANDLE_VALUE) {
        CloseHandle(_handle);
        _was_released = true;
    }
}

bool mmap::raii::WinHandle::valid() const noexcept
{
    return _handle != INVALID_HANDLE_VALUE;
}

PHANDLE mmap::raii::WinHandle::ptr() const noexcept
{
    return const_cast<PHANDLE>(&_handle);
}

mmap::raii::WinHandle::operator HANDLE() noexcept
{
    return _handle;
}
