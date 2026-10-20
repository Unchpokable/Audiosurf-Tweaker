#pragma once

// A script's own files: tw.storage (Docs/Internal/script-tabs.md §3.8). The extern "C" half the prelude
// binds (prelude/storage.lua), each a pass into lua_storage, which owns the folders, the limits, the writer
// thread and the JSON tape. See api_channels.hxx for why everything here looks like C.
//
// Every call that can fail returns 0 and leaves the reason for tw_storage_error(); the prelude hands it to the
// script as `nil, reason`. Pointers handed out stay valid until the next tw_storage_* call - the prelude copies
// out of them at once.
namespace tw::lua::api
{
extern "C" {
// The bytes of a file, as they are - text or not.
int tw_storage_read(int owner, const char* name, const char** data, int* size) noexcept;
int tw_storage_write(int owner, const char* name, const char* data, int size) noexcept;

// JSON as a tape of lua_storage::json_node, pre-order (lua_storage.hxx). `tape` is a json_node*; the prelude
// declares the same struct.
int tw_storage_read_json(int owner, const char* name, const void** tape, int* count) noexcept;
int tw_storage_write_json(int owner, const char* name, const void* tape, int count) noexcept;

int tw_storage_exists(int owner, const char* name) noexcept;
int tw_storage_remove(int owner, const char* name) noexcept;

// The script's files, sorted, one name per line.
const char* tw_storage_list(int owner) noexcept;

// Why the last call that returned 0 did.
const char* tw_storage_error() noexcept;
}
} // namespace tw::lua::api
