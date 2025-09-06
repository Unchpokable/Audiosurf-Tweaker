using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using Tweaker.Core.Errors;

namespace Tweaker.Core.Skins;

[StructLayout(LayoutKind.Sequential, Pack = 4)]
public struct Header
{
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 16)]
    public string Signature;

    public UInt16 Major;
    public UInt16 Minor;
    public UInt16 Patch;
}

public static class Packager
{
    private static ReadOnlySpan<byte> ExpectedSignature => "TWEAKER_SKINDDDD"u8;

    private static readonly Header RequiredHeader = new()
    {
        Major = 1,
        Minor = 0,
        Patch = 0
    };

    public static Result<TexturePackData> Load(string path)
    {
        if (!File.Exists(path))
        {
            return new FileNotFoundException().ToFailure<TexturePackData>();
        }

        var stream = File.OpenRead(path);

        return ReadStruct<Header>(stream).Match(
            onSuccess: header =>
            {
                if (!IsFileCompatible(header))
                {
                    return "Incompatible save!".ToFailure<TexturePackData>();
                }

                return LoadInternal(stream);
            },
            onError: error => $"Can not read file. Whats happened: {error.Message}".ToFailure<TexturePackData>(error.InnerException));
    }

    public static Result<bool> Save(string path, TexturePackData data)
    {
        var stream = File.OpenWrite(path);

        Header header;
        header.Signature = ExpectedSignature.ToString();
        header.Major = RequiredHeader.Major;
        header.Minor = RequiredHeader.Minor;
        header.Patch = RequiredHeader.Patch;

        WriteStruct(stream, header);

        return SaveInternal(stream, data);
    }

    private static Result<T> ReadStruct<T>(Stream stream) where T : struct
    {
        try
        {
            var size = Marshal.SizeOf<T>();
            byte[] buffer = new byte[size];
            var bytesRead = stream.Read(buffer, 0, size);

            if (bytesRead != size)
            {
                return "Can not read stream - less bytes than header struct awaits".ToFailure<T>();
            }

            var handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);

            try
            {
                return Marshal.PtrToStructure<T>(handle.AddrOfPinnedObject()).ToSuccess();
            }
            finally
            {
                handle.Free();
            }
        }
        catch (Exception ex)
        {
            return ex.ToFailure<T>();
        }
    }

    private static void WriteStruct<T>(Stream stream, T structure) where T : struct
    {
        var size = Marshal.SizeOf<T>();
        byte[] buffer = new byte[size];

        IntPtr ptr = Marshal.AllocHGlobal(size);

        try
        {
            Marshal.StructureToPtr(structure, ptr, false);
            Marshal.Copy(ptr, buffer, 0, size);
            stream.Write(buffer);
        }
        finally
        {
            Marshal.FreeHGlobal(ptr);
        }
    }

    private static bool IsFileCompatible(Header header)
    {
        bool compatible = true;

        ReadOnlySpan<byte> signatureBytes = Encoding.ASCII.GetBytes(header.Signature.TrimEnd('\0'));

        compatible = compatible && signatureBytes.SequenceEqual(ExpectedSignature);
        compatible = compatible || header.Major == RequiredHeader.Major;

        return compatible;
    }

    private static unsafe Result<bool> SaveInternal(Stream stream, TexturePackData data)
    {
        ulong requiredPartsCount = (ulong)data.RequiredParts.Count;
        stream.Write(new ReadOnlySpan<byte>(&requiredPartsCount, sizeof(ulong)));

        foreach (var image in data.RequiredParts)
        {
            var written = image.WriteToStream(stream);
            if (written.Failed())
            {
                return written.Reason()?.ToFailure<bool>() ?? "WTF Failure".ToFailure<bool>();
            }
        }

        ulong optionalPartsCount = (ulong)data.OptionalParts.Count;
        stream.Write(new ReadOnlySpan<byte>(&optionalPartsCount, sizeof(ulong)));

        foreach (var image in data.OptionalParts)
        {
            var written = image.WriteToStream(stream);
            if (written.Failed())
            {
                return written.Reason()?.ToFailure<bool>() ?? "WTF Failure".ToFailure<bool>();
            }
        }

        ulong previewsCount = (ulong)data.Previews.Count;
        stream.Write(new ReadOnlySpan<byte>(&previewsCount, sizeof(ulong)));

        foreach (var image in data.Previews)
        {
            var written = image.WriteToStream(stream);
            if (written.Failed())
            {
                return written.Reason()?.ToFailure<bool>() ?? "WTF Failure".ToFailure<bool>();
            }
        }

        return true.ToSuccess();
    }

    private static unsafe Result<TexturePackData> LoadInternal(Stream stream)
    {
        var data = new TexturePackData();

        ulong requiredPartsCount;
        stream.ReadExactly(new Span<byte>(&requiredPartsCount, sizeof(ulong)));

        for (ulong i = 0; i < requiredPartsCount; i++)
        {
            NamedImage image = new();
            var read = image.ReadFromStream(stream);
            if (read.Failed())
            {
                return read.Reason()?.ToFailure<TexturePackData>() ?? "WTF Failure".ToFailure<TexturePackData>();
            }
        }

        ulong optionalPartsCount;
        stream.ReadExactly(new Span<byte>(&optionalPartsCount, sizeof(ulong)));

        for (ulong i = 0; i < optionalPartsCount; i++)
        {
            NamedImage image = new();
            var read = image.ReadFromStream(stream);
            if (read.Failed())
            {
                return read.Reason()?.ToFailure<TexturePackData>() ?? "WTF Failure".ToFailure<TexturePackData>();
            }
        }

        ulong previewsCount;
        stream.ReadExactly(new Span<byte>(&previewsCount, sizeof(ulong)));

        for (ulong i = 0; i <= previewsCount; i++)
        {
            NamedImage image = new();
            var read = image.ReadFromStream(stream);
            if (read.Failed())
            {
                return read.Reason()?.ToFailure<TexturePackData>() ?? "WTF Failure".ToFailure<TexturePackData>();
            }
        }

        return data.ToSuccess();
    }
}
