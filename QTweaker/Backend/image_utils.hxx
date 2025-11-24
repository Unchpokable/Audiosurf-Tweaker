#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

#include "Backend_global.h"

namespace utils::image
{
BACKEND_EXPORT QImage resize(QImage& old, int new_width, int new_height);
BACKEND_EXPORT QImage blur(const QImage& src, std::size_t iter_count = 1);
} // namespace utils::image

#endif // IMAGE_UTILS_H
