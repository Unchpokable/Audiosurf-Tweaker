using System.Runtime.InteropServices;

namespace Tweaker.Core.PInvoke;

public enum Result : int
{
    Success = 0,
    FileNotFound,
    UnknownFormat,
    DataCorrupted,
    OutOfMemory,
    UnsupportedOperation,
    UnknownError
}

public enum NativeFormat
{
    Jpeg,
    Png,
    Unsupported
}

public static class ImageUtils
{
    private const string DllName = "Tweaker.Native.dll"; // Замени на имя своей библиотеки

    [DllImport(DllName, EntryPoint = "generic_decode", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern Result GenericDecode(
       [MarshalAs(UnmanagedType.LPStr)] string filePath,
       out IntPtr data,
       out int width,
       out int height,
       out int channels);

    [DllImport(DllName, EntryPoint = "encode_png", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern Result EncodePng(
        [MarshalAs(UnmanagedType.LPStr)] string filePath,
        IntPtr data,
        int width,
        int height,
        int channels);

    [DllImport(DllName, EntryPoint = "encode_jpeg", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern Result EncodeJpeg(
        [MarshalAs(UnmanagedType.LPStr)] string filePath,
        IntPtr data,
        int width,
        int height,
        int channels,
        int quality);

    [DllImport(DllName, EntryPoint = "free_image", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern Result FreeImage(IntPtr data);

    [DllImport(DllName, EntryPoint = "get_image_format_native", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern NativeFormat GetImageFormatNative([MarshalAs(UnmanagedType.LPStr)] string filename);
}