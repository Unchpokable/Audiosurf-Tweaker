#pragma once

namespace mmap::raii
{
struct WinHandle final {
    template<auto TFunc, typename... TArgs>
    static WinHandle create(TArgs... args) noexcept;

    WinHandle(HANDLE handle) noexcept;
    WinHandle() noexcept;
    ~WinHandle() noexcept;

    WinHandle(const WinHandle&) = delete;
    WinHandle& operator=(const WinHandle&) = delete;

    WinHandle(WinHandle&& other) noexcept;
    WinHandle& operator=(WinHandle&& other) noexcept;

    void release() noexcept;
    bool valid() const noexcept;

    PHANDLE ptr() const noexcept;

    operator HANDLE() noexcept;

private:
    HANDLE _handle;

    bool _was_moved;
    bool _was_released;
};
} // namespace mmap::raii

template<auto TFunc, typename... TArgs>
mmap::raii::WinHandle mmap::raii::WinHandle::create(TArgs... args) noexcept
{
    auto handle = TFunc(std::forward<TArgs>(args)...);

    return WinHandle(handle);
}
