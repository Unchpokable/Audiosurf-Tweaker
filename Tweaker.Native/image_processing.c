#define _CRT_SECURE_NO_WARNINGS

#include "image_processing.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define DONT_CARE(value) (void)value

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

TW_NATIVE_API Result generic_decode(const char* file_path, unsigned char** data, int32_t* width, int32_t* height, int32_t* channels)
{
    stbi_uc* img = stbi_load(file_path, width, height, channels, 0);

    if(img == NULL) {
        return stbi_convert_error();
    }

    memcpy(*data, img, (size_t)(*width) * (*height));

    free(img);

    return Success;
}

TW_NATIVE_API Result encode_png(const char* file_path, const unsigned char* data, int32_t width, int32_t height, int32_t channels)
{
    stbi_write_png_compression_level = 8;

    int failed = stbi_write_png(file_path, width, height, channels, data, 0);

    if(failed) {
        return OutOfMemory;
    }

    return Success;
}

TW_NATIVE_API Result encode_jpeg(
    const char* file_path, const unsigned char* data, int32_t width, int32_t height, int32_t channels, int32_t quality)
{
    int failed = stbi_write_jpg(file_path, width, height, channels, data, quality);

    if(failed) {
        return UnknownError;
    }

    return Success;
}

TW_NATIVE_API NativeFormat get_image_format_native(const char* file_path)
{
    int width, height, channels;

    if(stbi_info(file_path, &width, &height, &channels)) {
        FILE* file = fopen(file_path, "rb");
        if(file == NULL) {
            return Unsupported;
        }

        unsigned char header[8];
        if(fread(header, 1, 8, file) != 8) {
            DONT_CARE(fclose(file));
            return Unsupported;
        }
        DONT_CARE(fclose(file));

        if(header[0] == 0xFF && header[1] == 0xD8) {
            return Jpeg;
        }

        if(header[0] == 0x89 && header[1] == 0x50 && header[2] == 0x4E && header[3] == 0x47) {
            return Png;
        }
    }

    return Unsupported;
}

TW_NATIVE_API void free_image(unsigned char* buffer)
{
    stbi_image_free(buffer);
}