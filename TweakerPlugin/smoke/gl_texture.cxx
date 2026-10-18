#include "gl_texture.hxx"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <GL/gl.h>

namespace tw::smoke
{
ImTextureID gl_upload_texture(const unsigned char* rgba, int width, int height)
{
    if(rgba == nullptr || width <= 0 || height <= 0) {
        return ImTextureID_Invalid;
    }

    // The buffer is tightly packed RGBA, and the unpack state that says so is set here rather than
    // assumed. This is called mid-frame (svg::at() rasterizes a non-baked size on first ask), after
    // ImGui's GL3 backend has updated the font atlas - and that backend (1.92.9 WIP, UpdateTexture)
    // leaves GL_UNPACK_ROW_LENGTH at the atlas width: its restore is guarded by tex->Status, which it
    // has already set to OK by then. Inheriting that, the driver reads a 19-pixel icon with a
    // 1024-pixel row pitch - garbage on screen, or an access violation inside the driver. Seen as the
    // notefeed toast icon breaking or crashing smoke_test (13.10.2026); scene "late-icon" reproduces it.
    GLint last_row_length = 0;
    GLint last_skip_rows = 0;
    GLint last_skip_pixels = 0;
    GLint last_alignment = 0;
    GLint last_texture = 0;
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &last_row_length);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &last_skip_rows);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &last_skip_pixels);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &last_alignment);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Explicit clamp (not the GL_REPEAT default) - default wrap mode on non-power-of-two textures
    // is where legacy/compatibility GL contexts are most likely to sample garbage at the edges.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(last_texture));
    glPixelStorei(GL_UNPACK_ROW_LENGTH, last_row_length);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, last_skip_rows);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, last_skip_pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, last_alignment);

    return tex != 0 ? static_cast<ImTextureID>(tex) : ImTextureID_Invalid;
}

void gl_release_texture(ImTextureID tex)
{
    const auto name = static_cast<GLuint>(tex);
    if(name != 0) {
        glDeleteTextures(1, &name);
    }
}
} // namespace tw::smoke
