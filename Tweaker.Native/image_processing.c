#define _CRT_SECURE_NO_WARNINGS

#include "image_processing.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static Result stbi_convert_error(void)
{
    const char* err = stbi_failure_reason();

    if(strstr(err, "can't fopen") || strstr(err, "fopen"))
        return FileNotFound;

    if(strstr(err, "unknown image type") || strstr(err, "not supported"))
        return UnknownFormat;

    if(strstr(err, "corrupt") || strstr(err, "bad") || strstr(err, "invalid"))
        return DataCorrupted;

    if(strstr(err, "out of memory") || strstr(err, "malloc"))
        return OutOfMemory;

    if(strstr(err, "unsupported"))
        return UnsupportedOperation;

    return UnknownError;
}

Result generic_decode(const char* file_path, unsigned char** data, int32_t* width, int32_t* height, int32_t* channels)
{
    stbi_uc* img = stbi_load(file_path, width, height, channels, 0);

    if(img == NULL) {
        return stbi_convert_error();
    }

    memcpy(*data, img, (size_t)(*width) * (*height));

    free(img);

    return Success;
}

Result encode_png(const char* file_path, const unsigned char* data, int32_t width, int32_t height, int32_t channels)
{
    stbi_write_png_compression_level = 8;

    int failed = stbi_write_png(file_path, width, height, channels, data, 0);

    if(failed) {
        return OutOfMemory;
    }

    return Success;
}

Result encode_jpeg(const char* file_path, const unsigned char* data, int32_t width, int32_t height, int32_t channels, int32_t quality)
{
    int failed = stbi_write_jpg(file_path, width, height, channels, data, quality);

    if(failed) {
        return UnknownError;
    }

    return Success;
}

void free_image(unsigned char* buffer)
{
    stbi_image_free(buffer);
}