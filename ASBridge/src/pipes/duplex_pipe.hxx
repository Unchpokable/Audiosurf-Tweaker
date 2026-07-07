#pragma once

#ifndef HT_DUPLEX_PIPE_HXX
#define HT_DUPLEX_PIPE_HXX

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

#include "pipes/file.hxx"
#include "pipes/handle.hxx"
#include "pipes/win32_overlapped.hxx"

namespace as
{
class duplex_pipe final {
public:
    static constexpr int timeout_infinite = -1;
    static constexpr std::size_t default_buffer_size = 4096;

    enum class io_result : std::uint8_t {
        ok,           //< write: all bytes accepted; read: message drained in full
        more_data,    //< read-only: message-mode ERROR_MORE_DATA, buffer was smaller than the message, call read_to again
        disconnected, //< peer closed its end (ERROR_BROKEN_PIPE / ERROR_PIPE_NOT_CONNECTED)
        pending,      //< overlapped_result(wait=false) only: operation has not completed yet
    };

    ///@brief Wraps an already open/connected handle (secondary path, mirrors process(proc_handle)).
    explicit duplex_pipe(as::file_handle handle, as::system_path fname = {});
    ~duplex_pipe();

    duplex_pipe(const duplex_pipe&) = delete;
    duplex_pipe(duplex_pipe&& other) noexcept;

    duplex_pipe& operator=(const duplex_pipe&) = delete;
    duplex_pipe& operator=(duplex_pipe&& other);

    bool valid() const noexcept;
    bool connected() const noexcept;
    bool is_server() const noexcept;

    ///@brief Server-side only: blocks (up to timeout) until a client connects.
    bool accept(int timeout = timeout_infinite);

    ///@brief Blocks with no timeout until the write completes. Do not call on a connection that may
    /// already be disconnected with no prior operation in flight - see read_to() for why.
    io_result put(std::span<const std::byte> content, std::size_t length, std::size_t& bytes_written);
    io_result put(const std::vector<std::byte>& content, std::size_t& bytes_written);

    ///@brief Blocks with no timeout until data arrives. A pending overlapped read that is in flight
    /// when the peer disconnects wakes up correctly (disconnected); one issued cold, strictly after
    /// the peer has already gone, can block forever with no recovery - a named-pipe/driver quirk, not
    /// something read_to() can detect in advance. Use wait_for_can_read() first if the peer's liveness
    /// is in doubt; it never blocks inside the kernel's overlapped machinery.
    io_result read_to(std::span<std::byte> buffer, std::size_t length, std::size_t& bytes_read);
    io_result read_to(std::vector<std::byte>& buffer, std::size_t length, std::size_t& bytes_read);

    std::vector<std::byte> read_all();

#if defined(HT_FILE_ON_WINDOWS)
    as::overlapped_ptr put_overlapped(std::span<const std::byte> content, std::size_t length);
    as::overlapped_ptr put_overlapped(const std::vector<std::byte>& content);

    as::overlapped_ptr read_to_overlapped(std::span<std::byte> buffer, std::size_t length);
    as::overlapped_ptr read_to_overlapped(std::vector<std::byte>& buffer);

    ///@brief Harvests the result of an operation issued via *_overlapped. wait=false polls without blocking.
    io_result overlapped_result(const as::overlapped_ptr& op, std::size_t& bytes_transferred, bool wait = false);
#endif

    bool wait_for_can_read(int timeout = timeout_infinite);

private:
    struct pending_ops {
        std::mutex mtx;
        std::vector<as::overlapped_ptr> ops;
    };

    duplex_pipe(as::file_handle handle, as::system_path fname, bool is_server, std::size_t buffer_size) noexcept;

    void teardown() noexcept;

    friend duplex_pipe create_duplex_pipe(as::system_path name, std::size_t buffer_size);
    friend duplex_pipe open_duplex_pipe(as::system_path name, int timeout);

    as::system_path m_fname;
    as::file_handle m_file { as::invalid_handle };

    std::size_t m_readwrite_length;
    flags_bitmask m_flags { 0 };

    bool m_is_server { false };
    bool m_connected { false };

    std::unique_ptr<pending_ops> m_pending;
};
} // namespace as

namespace as
{
duplex_pipe create_duplex_pipe(as::system_path name, std::size_t buffer_size = duplex_pipe::default_buffer_size);
duplex_pipe open_duplex_pipe(as::system_path name, int timeout = duplex_pipe::timeout_infinite);
} // namespace as

#endif
