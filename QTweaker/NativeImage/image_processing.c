#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

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

TW_NATIVE_API Result image_info(const char* file_path, int32_t* width, int32_t* height, int32_t* channels)
{
    if(stbi_info(file_path, width, height, channels)) {
        return stbi_convert_error();
    }

    return Success;
}


TW_NATIVE_API Result image_info_from_mem(const unsigned char* data, int32_t data_size, int32_t* width, int32_t* height, int32_t* channels)
{
    if(stbi_info_from_memory(data, data_size, width, height, channels)) {
        return stbi_convert_error();
    }

    return Success;
}

TW_NATIVE_API Result generic_decode(const char* file_path, unsigned char** data, int32_t* width, int32_t* height, int32_t* channels)
{
    stbi_uc* img = stbi_load(file_path, width, height, channels, 0);

    if(img == NULL) {
        return stbi_convert_error();
    }

    *data = img;

    return Success;
}

TW_NATIVE_API Result decode_from_mem(const unsigned char* data, int32_t data_len, unsigned char** output_data, int32_t* width, int32_t* height, int32_t* channels)
{
    stbi_uc* img = stbi_load_from_memory(data, data_len, width, height, channels, 0);

    if(img == NULL) {
        return stbi_convert_error();
    }

    *output_data = img;

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

TW_NATIVE_API NativeFormat image_format(const char* file_path)
{
    FILE* file = fopen(file_path, "rb");
    if(file == NULL) {
        return Unsupported;
    }

    stbi__context stb_context;
    stbi__start_file(&stb_context, file);

    NativeFormat format = Unsupported;

    if(stbi__png_test(&stb_context)) {
        format = Png;
    }

    if(stbi__jpeg_test(&stb_context)) {
        format = Jpeg;
    }

    DONT_CARE(fclose(file));

    return format;
}

NativeFormat image_format_from_mem(const unsigned char* data, int32_t data_size)
{
    if(data == NULL || data_size <= 0) {
        return Unsupported;
    }

    stbi__context stb_context;
    stbi__start_mem(&stb_context, data, data_size);

    if(stbi__png_test(&stb_context)) {
        return Png;
    }

    if(stbi__jpeg_test(&stb_context)) {
        return Jpeg;
    }

    return Unsupported;
}

TW_NATIVE_API void free_image(unsigned char* buffer)
{
    stbi_image_free(buffer);
}
