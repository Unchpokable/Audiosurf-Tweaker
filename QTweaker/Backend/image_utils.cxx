#include "precompiled.hxx"

#include "image_utils.hxx"

namespace
{
constexpr int blur_radius = 3;

void box_blur_horizontal(const QImage& src, QImage& dst)
{
    const int w = src.width();
    const int h = src.height();

    for(int y = 0; y < h; ++y) {
        const QRgb* src_line = reinterpret_cast<const QRgb*>(src.scanLine(y));
        QRgb* dst_line = reinterpret_cast<QRgb*>(dst.scanLine(y));

        int sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;

        for(int kx = -blur_radius; kx <= blur_radius; ++kx) {
            int ix = std::clamp(kx, 0, w - 1);
            QRgb pixel = src_line[ix];
            sum_r += qRed(pixel);
            sum_g += qGreen(pixel);
            sum_b += qBlue(pixel);
            sum_a += qAlpha(pixel);
        }

        const int kernel_size = blur_radius * 2 + 1;

        for(int x = 0; x < w; ++x) {
            dst_line[x] = qRgba(sum_r / kernel_size, sum_g / kernel_size, sum_b / kernel_size, sum_a / kernel_size);

            int left_idx = std::clamp(x - blur_radius, 0, w - 1);
            int right_idx = std::clamp(x + blur_radius + 1, 0, w - 1);

            QRgb left_pixel = src_line[left_idx];
            QRgb right_pixel = src_line[right_idx];

            sum_r += qRed(right_pixel) - qRed(left_pixel);
            sum_g += qGreen(right_pixel) - qGreen(left_pixel);
            sum_b += qBlue(right_pixel) - qBlue(left_pixel);
            sum_a += qAlpha(right_pixel) - qAlpha(left_pixel);
        }
    }
}

void box_blur_vertical(const QImage& src, QImage& dst)
{
    const int w = src.width();
    const int h = src.height();

    for(int x = 0; x < w; ++x) {
        int sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;

        for(int ky = -blur_radius; ky <= blur_radius; ++ky) {
            int iy = std::clamp(ky, 0, h - 1);
            QRgb pixel = reinterpret_cast<const QRgb*>(src.scanLine(iy))[x];
            sum_r += qRed(pixel);
            sum_g += qGreen(pixel);
            sum_b += qBlue(pixel);
            sum_a += qAlpha(pixel);
        }

        const int kernel_size = blur_radius * 2 + 1;

        for(int y = 0; y < h; ++y) {
            QRgb* dst_line = reinterpret_cast<QRgb*>(dst.scanLine(y));
            dst_line[x] = qRgba(sum_r / kernel_size, sum_g / kernel_size, sum_b / kernel_size, sum_a / kernel_size);

            int top_idx = std::clamp(y - blur_radius, 0, h - 1);
            int bottom_idx = std::clamp(y + blur_radius + 1, 0, h - 1);

            QRgb top_pixel = reinterpret_cast<const QRgb*>(src.scanLine(top_idx))[x];
            QRgb bottom_pixel = reinterpret_cast<const QRgb*>(src.scanLine(bottom_idx))[x];

            sum_r += qRed(bottom_pixel) - qRed(top_pixel);
            sum_g += qGreen(bottom_pixel) - qGreen(top_pixel);
            sum_b += qBlue(bottom_pixel) - qBlue(top_pixel);
            sum_a += qAlpha(bottom_pixel) - qAlpha(top_pixel);
        }
    }
}
} // namespace

QImage utils::image::resize(QImage& old, int new_width, int new_height)
{
    if(old.isNull() || new_width == 0 || new_height == 0) {
        return old;
    }

    QImage new_image = old.scaled(new_width, new_height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return new_image;
}

QImage utils::image::blur(const QImage& src, std::size_t iter_count)
{
    if(iter_count == 0 || src.isNull()) {
        return src;
    }

    QImage result = src.convertToFormat(QImage::Format_ARGB32);
    QImage temp(result.size(), QImage::Format_ARGB32);

    for(std::size_t i = 0; i < iter_count; ++i) {
        box_blur_horizontal(result, temp);
        box_blur_vertical(temp, result);
    }

    return result;
}
