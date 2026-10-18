#pragma once

#include <imgui.h>

// gpu_texture's pluggable backend (see ui/gpu_texture.hxx) for smoke_test. The DLL registers the D3D9
// pair (framework/imgui_backend.cxx); this is the GL equivalent, so the exact same overlay code loads
// real packed icons in both the interactive window and the snapshot stand. Needs a current GL context.
namespace tw::smoke
{
ImTextureID gl_upload_texture(const unsigned char* rgba, int width, int height);
void gl_release_texture(ImTextureID tex);
} // namespace tw::smoke
