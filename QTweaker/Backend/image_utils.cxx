#include "precompiled.hxx"

#include "image_utils.hxx"

QImage utils::image::resize(QImage& old, int new_width, int new_height)
{
    QImage new_image = old.scaled(new_width, new_height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return new_image;
}

QImage utils::image::blur(const QImage& src, float sigma)
{
    // todo: implement Gaussian blur
    return src;
}
