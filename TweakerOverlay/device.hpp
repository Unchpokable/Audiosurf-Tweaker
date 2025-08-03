#pragma once

namespace tw::native
{
enum DxFunction : std::uint16_t {
    Reset = 16,
    Present = 17,
    EndScene = 42,
};
} // namespace tw::native

namespace tw::native
{
enum HookResult {
    Success,        /// Hook attached successfully
    Overridden,     /// Hook attached successfully, but previous hook was overriden and no more valid
    GenericFailure, /// something exploded
    WrongPointer,   /// pointer to hook was wrong (mostly - nullptr)
    AlreadyInUse,   /// This function already hooked
    NotFound,       /// Detours can not find a function or passed function vtable address is invalid
};
} // namespace tw::native

namespace tw::native
{
extern IDirect3DDevice9* current_device;
} // namespace tw::native

namespace tw::native
{
void change_device(IDirect3DDevice9* device);
} // namespace tw::native

namespace tw::native
{
HookResult set_hook(DxFunction function, void* hook);
HookResult remove_hook(DxFunction function);
HookResult remove_all_hooks();
} // namespace tw::native
