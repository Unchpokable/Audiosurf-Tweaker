using System.IO.Compression;
using System.Runtime.InteropServices;
using System.Windows;
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
        Generic
    }

    public string Name { get; set; }
    public string Description { get; set; }
    public Format Type { get; private set; }
    public Vec2<int> Geometry { get; private set; }
    public int ChannelsCount { get; private set; }

    private readonly byte[] _data;

    public static Result<NamedImage> FromFile(string filename)
    {
        var result = GenericDecode(filename, out IntPtr bufferPtr, out int width, out int height, out int channelsCount);

        if (result != Result.Success)
        {
            return new Failure<NamedImage>(new Error($"Can not load image: { result }"));
        }

        int totalBytes = width * height * channelsCount;
        byte[] imageData = new byte[totalBytes];
        
        Marshal.Copy(bufferPtr, imageData, 0, totalBytes);
        
        FreeImage(bufferPtr);

        return new NamedImage(filename, Format.Generic, new Vec2<int> { X = width, Y = height }, imageData, channelsCount).ToSuccess();
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
}