namespace tw::game
{
void initialize();
} // namespace tw::game

namespace tw::game
{
IDirect3D9* get_direct3d(void* channel);
IDirect3DDevice9* get_device(void* channel);
} // namespace tw::game