#pragma once

#ifdef _WIN32
#ifdef TWEAKER_NATIVE_EXPORT
#define TW_NATIVE_API __declspec(dllexport)
#else
#define TW_NATIVE_API __declspec(dllimport)
#endif
#else
#define TW_NATIVE_API __attribute__((visibility("default")))
#endif

#include <stdint.h>

typedef enum
{
    Success = 0,
    FileNotFound,
    UnknownFormat,
    DataCorrupted,
    OutOfMemory,
    UnsupportedOperation,
    UnknownError
} Result;

typedef enum
{
    Jpeg,
    Png,
    Unsupported,
} NativeFormat;

TW_NATIVE_API Result generic_decode(const char* file_path, unsigned char** data, int32_t* width, int32_t* height, int32_t* channels);

TW_NATIVE_API Result encode_png(const char* file_path, const unsigned char* data, int32_t width, int32_t height, int32_t channels);
TW_NATIVE_API Result encode_jpeg(const char* file_path, const unsigned char* data, int32_t width, int32_t height, int32_t channels, int32_t quality);

TW_NATIVE_API NativeFormat get_image_format_native(const char* file_path);

TW_NATIVE_API void free_image(unsigned char* buffer);
