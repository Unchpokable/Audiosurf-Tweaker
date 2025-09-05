using System.IO;
using System.IO.Compression;
using System.Runtime.InteropServices;
using Tweaker.Core.Errors;
using Tweaker.Core.PInvoke;

using static Tweaker.Core.PInvoke.ImageUtils;

namespace Tweaker.Core;

#nullable disable

public sealed class NamedImage
{
    public enum Format
    {
        Jpeg,
        Png,
        Unsupported
    }

    public string Name { get; set; }
    public string Description { get; set; }
    public Format Type { get; }
    public Vec2<int> Geometry { get; }
    public int ChannelsCount { get; }

    private readonly byte[] _data;

    public static Result<NamedImage> FromFile(string filename)
    {
        var format = (Format)GetImageFormatNative(filename);
        if (format == Format.Unsupported)
        {
            return $"Wrong Format!".ToFailure<NamedImage>();
        }

        var result = GenericDecode(filename, out IntPtr bufferPtr, out int width, out int height, out int channelsCount);

        if (result != Result.Success)
        {
            return new Failure<NamedImage>(new Error($"Can not load image: {result}"));
        }

        var totalBytes = width * height * channelsCount;
        var imageData = new byte[totalBytes];

        Marshal.Copy(bufferPtr, imageData, 0, totalBytes);

        FreeImage(bufferPtr);

        var compressedData = CompressData(imageData);

        return new NamedImage(filename, format, new Vec2<int> { X = width, Y = height }, compressedData, channelsCount).ToSuccess();
    }

    private NamedImage(string name, Format type, Vec2<int> geometry, byte[] data, int channelsCount, string description = null)
    {
        Name = name;
        Type = type;
        Geometry = geometry;
        ChannelsCount = channelsCount;

        _data = data;
        Description = description;
    }

    public Result<bool> SaveTo(string filePath, Format format)
    {
        if (!Directory.Exists(filePath))
        {
            return "Given directory does not exists!".ToFailure<bool>();
        }

        return FormatToExtension(Type).Match(
            onSuccess: extension =>
            {
                var completedFileName = Path.Combine(filePath, Name, extension);

                return WriteInternal(completedFileName, format);
            },
            onError: error => $"Unsupported image type! Internal message: {error.Message}".ToFailure<bool>());
    }

    private static byte[] CompressData(byte[] data)
    {
        using var output = new MemoryStream();
        using var zip = new DeflateStream(output, CompressionLevel.SmallestSize);

        zip.Write(data, 0, data.Length);
        zip.Close();

        return output.ToArray();
    }

    private static byte[] DecompressData(byte[] compressedData)
    {
        using var input = new MemoryStream(compressedData);
        using var zip = new DeflateStream(input, CompressionMode.Decompress);
        using var output = new MemoryStream();
        zip.CopyTo(output);
        return output.ToArray();
    }

    private static Result<string> FormatToExtension(Format format)
    {
        return format switch
        {
            Format.Png => ".png".ToSuccess(),
            Format.Jpeg => ".png".ToSuccess(),
            _ => "unknown".ToFailure<string>()
        };
    }

    private Result<bool> WriteInternal(string filePath, Format format)
    {
        var decompressedData = DecompressData(_data);

        if (decompressedData == null || decompressedData.Length == 0)
        {
            return "Can not decompress data".ToFailure<bool>();
        }

        IntPtr unmanagedBufferPtr = Marshal.AllocHGlobal(decompressedData.Length);
        Marshal.Copy(decompressedData, 0, unmanagedBufferPtr, decompressedData.Length);

        var nativeCode = format switch
        {
            Format.Png => EncodePng(filePath, unmanagedBufferPtr, Geometry.X, Geometry.Y, ChannelsCount),
            Format.Jpeg => EncodeJpeg(filePath, unmanagedBufferPtr, Geometry.X, Geometry.Y, ChannelsCount, 95),
            _ => Result.Success
        };

        return nativeCode switch
        {
            Result.Success => true.ToSuccess(),
            _ => $"Failed to save image! Whats happened: {nativeCode}".ToFailure<bool>()
        };
    }
}