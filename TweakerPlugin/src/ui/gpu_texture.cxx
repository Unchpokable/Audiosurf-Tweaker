#include "ui/gpu_texture.hxx"

namespace
{
tw::ui::gpu_texture::upload_fn g_upload = nullptr;
tw::ui::gpu_texture::release_fn g_release = nullptr;
} // namespace

namespace tw::ui::gpu_texture
{
void set_backend(upload_fn upload, release_fn release) noexcept
{
    g_upload = upload;
    g_release = release;
}

bool has_backend() noexcept
{
    return g_upload != nullptr;
}

ImTextureID upload(const unsigned char* rgba, int width, int height) noexcept
{
    if(g_upload == nullptr || rgba == nullptr || width <= 0 || height <= 0) {
        return ImTextureID_Invalid;
    }
    return g_upload(rgba, width, height);
}

void release(ImTextureID tex) noexcept
{
    if(g_release == nullptr || tex == ImTextureID_Invalid) {
        return;
    }
    g_release(tex);
}
} // namespace tw::ui::gpu_texture
