using System.IO;
using System.IO.Compression;
using System.Runtime.InteropServices;
using System.Text;
using Tweaker.Core.Errors;
using Tweaker.Core.PInvoke;

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
            return "File not Found!".ToFailure<TexturePackData>("NO_FILE", new FileNotFoundException());
        }

        var stream = File.OpenRead(path);

        return ReadStruct<Header>(stream).Match(
            onSuccess: header =>
            {
                return new TexturePackData().ToSuccess();
            },
            onError: error => $"Can not read file. Whats happened: {error.Message}".ToFailure<TexturePackData>());
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

        using var inlineStream = new MemoryStream();
        using var deflateStream = new DeflateStream(inlineStream, CompressionLevel.Optimal);


    }

    private static Result<T> ReadStruct<T>(Stream stream) where T : struct
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
}
