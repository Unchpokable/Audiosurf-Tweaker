#pragma once

namespace tw::resource
{
enum class type {
    texture, // raster, decoded by stb_image (ui/texture_cache.cxx)
    vector,  // SVG source, rasterized by lunasvg (ui/image/svg.cxx)
    font,
    text,
    shader, // compiled D3D9 shader bytecode (.fxo), produced by fxc at build time
};
} // namespace tw::resource
