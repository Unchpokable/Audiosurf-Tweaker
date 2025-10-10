#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

#include "Backend_global.h"

namespace utils::image
{
BACKEND_EXPORT QImage resize(QImage& old, int new_width, int new_height);
BACKEND_EXPORT QImage blur(const QImage& src, float sigma);
}

#endif // IMAGE_UTILS_H
