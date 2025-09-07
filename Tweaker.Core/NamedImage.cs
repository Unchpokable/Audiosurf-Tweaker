using System.IO;
using System.IO.Compression;
using System.Runtime.InteropServices;
using System.Text;
using Tweaker.Core.Errors;
using Tweaker.Core.PInvoke;

using static Tweaker.Core.PInvoke.ImageUtils;

namespace Tweaker.Core;

#nullable disable

public sealed class NamedImage
{
    public enum Format : int
    {
        Jpeg,
        Png,
        Unsupported
    }

    public string Name { get; set; }
    public string Description { get; set; }
    public Format Type { get; private set; }
    public Vec2<int> Geometry { get; private set; }
    public int ChannelsCount { get; private set; }

    private byte[] _data;

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

    public NamedImage() {  }

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

                return WriteToFileInternal(completedFileName, format);
            },
            onError: error => $"Unsupported image type! Internal message: {error.Message}".ToFailure<bool>());
    }

    public unsafe Result<bool> ReadFromStream(Stream stream)
    {
        try
        {
            int nameSize = 0;
            stream.ReadExactly(new Span<byte>(&nameSize, sizeof(int)));

            byte[] stringBytes = new byte[nameSize];
            stream.ReadExactly(stringBytes, 0, nameSize);

            Name = Encoding.UTF8.GetString(stringBytes);

            int descriptionSize = 0;
            stream.ReadExactly(new Span<byte>(&descriptionSize, sizeof(int)));

            if (descriptionSize > 0)
            {
                byte[] descriptionBytes = new byte[descriptionSize];
                stream.ReadExactly(descriptionBytes, 0, descriptionSize);
                Description = Encoding.UTF8.GetString(descriptionBytes);
            }

            int rawType = 0;
            stream.ReadExactly(new Span<byte>(&rawType, sizeof(int)));

            Type = (Format)rawType;

            int width, height;
            stream.ReadExactly(new Span<byte>(&width, sizeof(int)));
            stream.ReadExactly(new Span<byte>(&height, sizeof(int)));

            Geometry = new Vec2<int> { X = width, Y = height };

            int channelsCount = 0;
            stream.ReadExactly(new Span<byte>(&channelsCount, sizeof(int)));

            ChannelsCount = channelsCount;

            int dataSize = 0;
            stream.ReadExactly(new Span<byte>(&dataSize, sizeof(int)));

            byte[] data = new byte[dataSize];
            stream.ReadExactly(data, 0, dataSize);

            _data = data;
        }
        catch (Exception ex)
        {
            return ex.ToFailure<bool>();
        }

        return true.ToSuccess();
    }

    public unsafe Result<bool> WriteToStream(Stream stream)
    {
        try
        {
            var nameBytes = Encoding.UTF8.GetBytes(Name);
            var nameBytesSize = nameBytes.Length * sizeof(byte);
            stream.Write(new ReadOnlySpan<byte>(&nameBytesSize, sizeof(int)));
            stream.Write(new ReadOnlySpan<byte>(nameBytes));

            if (Description != null)
            {
                var descriptionBytes = Encoding.UTF8.GetBytes(Description);
                var descriptionBytesSize = descriptionBytes.Length * sizeof(byte);
                stream.Write(new ReadOnlySpan<byte>(&descriptionBytesSize, sizeof(int)));
                stream.Write(new ReadOnlySpan<byte>(descriptionBytes));
            }
            else
            {
                int zero = 0;
                stream.Write(new ReadOnlySpan<byte>(&zero, sizeof(int)));
            }

            int rawType = (int)Type;
            stream.Write(new ReadOnlySpan<byte>(&rawType, sizeof(int)));

            int width = Geometry.X;
            int height = Geometry.Y;

            stream.Write(new ReadOnlySpan<byte>(&width, sizeof(int)));
            stream.Write(new ReadOnlySpan<byte>(&height, sizeof(int)));

            int channelsCount = ChannelsCount;
            stream.Write(new ReadOnlySpan<byte>(&channelsCount, sizeof(int)));

            int dataSize = _data.Length * sizeof(byte);
            stream.Write(new ReadOnlySpan<byte>(&dataSize, sizeof(int)));
            stream.Write(_data);
        }
        catch (Exception ex)
        {
            return ex.ToFailure<bool>();
        }

        return true.ToSuccess();
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

    private Result<bool> WriteToFileInternal(string filePath, Format format)
    {
        try
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
                _ => Result.UnknownFormat
            };

            Marshal.FreeHGlobal(unmanagedBufferPtr);

            return nativeCode switch
            {
                Result.Success => true.ToSuccess(),
                _ => $"Failed to save image! Whats happened: {nativeCode}".ToFailure<bool>()
            };
        }
        catch (Exception ex)
        {
            return ex.ToFailure<bool>();
        }
        
    }
}