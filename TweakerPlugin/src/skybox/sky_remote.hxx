#pragma once

// SKY_* operations over the existing TW_OVL transport. WndProc parses and queues; pump executes
// at the sky UI's frame boundary. Both are on Quest3D's main thread: this is deferred execution,
// not an inbound worker. The mailbox also tolerates reentrant window-message delivery.
namespace tw::skybox::remote
{
using send_fn = bool (*)(std::string_view line);
using present_fn = bool (*)() noexcept;

void set_link(send_fn send, present_fn present) noexcept;

void handle_op(std::string_view op, std::string_view rest);

void request_sync();

void pump() noexcept;

[[nodiscard]] std::string describe();
} // namespace tw::skybox::remote
