#pragma once

namespace tw::resource
{
enum class type {
    texture, // raster, decoded by stb_image (ui/texture_cache.cxx)
    vector,  // SVG source, rasterized by lunasvg (ui/image/svg.cxx)
    font,
    text,
};
} // namespace tw::resource
