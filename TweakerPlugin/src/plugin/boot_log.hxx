#pragma once

// The plugin's lifecycle log: engine\TweakerStuff\Logs\TweakerPlugin.log, written in Release as well.
//
// TW_LOG_* stays what it was - debug-only chatter, compiled out of the build that ships. This is the
// narrow channel next to it for the one kind of failure that otherwise leaves nothing behind: a plugin that
// loads from engine\channels\ on its own, with no host and no console, and does not come up. What goes
// here is listed in Docs/Internal/plugin-offline-mode.md §4.6 - startup stages and mode, modules found,
// pinned and hooked, first device, handshake.
//
// Where it may be called from: the startup thread, the IPC thread, DllMain and loader notifications (the
// writer is kernel32 only, formats into a stack buffer and never waits), and the render thread during
// initialisation only - the CreateDevice/Direct3DCreate9/DirectInput8Create chains, bind_device, Reset.
// NEVER from per-frame code: a WriteFile per frame is exactly the stutter §4.5 forbids.
namespace tw::plugin::boot_log
{
enum class open_mode : std::uint8_t {
    // The copy that owns this process: a log grown past its limit is started over.
    owner,
    // A second copy that found the owner already loaded: appends, never truncates the owner's log.
    guest,
};

// Creates TweakerStuff\Logs if needed and opens the file for appending. Loader-lock safe. Lines written
// before this, or when it fails, are dropped.
void open(const std::filesystem::path& logs_dir, open_mode mode) noexcept;

// One line, prefixed with the time since the process started and the calling thread id.
void write_line(std::string_view text) noexcept;

template<typename... TArgs>
void write(std::format_string<TArgs...> fmt, TArgs&&... args) noexcept
{
    // A fixed buffer rather than std::format: no allocation, so no heap traffic from inside the loader
    // either. A line longer than this is cut, which for a log line is the right failure.
    std::array<char, 768> buffer;
    const auto result = std::format_to_n(buffer.data(), buffer.size(), fmt, std::forward<TArgs>(args)...);
    const auto length = static_cast<std::size_t>(result.out - buffer.data());
    write_line(std::string_view { buffer.data(), length });
}
} // namespace tw::plugin::boot_log

// Expands straight into the call for the same reason diagnostics.hxx gives for TW_LOG_*: MSVC's
// traditional preprocessor hands a forwarded __VA_ARGS__ on as a single argument.
#define TW_BOOT_LOG(...) ::tw::plugin::boot_log::write(__VA_ARGS__)
