using System;
using System.IO;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace QuickPlayerCore.MetadataParsers
{
    internal static partial class NativeAudioMetadata
    {
        internal sealed record Snapshot(string Title, string Artist, string Album, TimeSpan Duration,
            int BitDepth, int SampleRate, int Bitrate);

        [StructLayout(LayoutKind.Sequential)]
        private struct Metadata
        {
            public IntPtr Title;
            public IntPtr Artist;
            public IntPtr Album;
            public long DurationMilliseconds;
            public int BitDepth;
            public int SampleRate;
            public int Bitrate;
        }

        internal static Snapshot Read(string path)
        {
            var status = ReadMetadata(path, out var metadata, out var pointer);
            using var snapshot = new SnapshotHandle(pointer);
            CheckStatus(status, path);

            return new Snapshot(Marshal.PtrToStringUni(metadata.Title), Marshal.PtrToStringUni(metadata.Artist),
                Marshal.PtrToStringUni(metadata.Album), TimeSpan.FromMilliseconds(metadata.DurationMilliseconds),
                metadata.BitDepth, metadata.SampleRate, metadata.Bitrate);
        }

        internal static void SetTitle(string path, string title) => CheckStatus(WriteTitle(path, title ?? ""), path);

        private static void CheckStatus(int status, string path)
        {
            if (status != 0)
                throw new IOException($"Native TagLib failed for '{path}' (status {status}).");
        }

        private sealed class SnapshotHandle : SafeHandleZeroOrMinusOneIsInvalid
        {
            internal SnapshotHandle(IntPtr pointer) : base(true) => SetHandle(pointer);

            protected override bool ReleaseHandle()
            {
                FreeMetadata(handle);
                return true;
            }
        }

        [LibraryImport("TweakerNative", EntryPoint = "read_metadata", StringMarshalling = StringMarshalling.Utf16)]
        private static partial int ReadMetadata(string path, out Metadata metadata, out IntPtr snapshot);

        [LibraryImport("TweakerNative", EntryPoint = "free_metadata")]
        private static partial void FreeMetadata(IntPtr snapshot);

        [LibraryImport("TweakerNative", EntryPoint = "write_title", StringMarshalling = StringMarshalling.Utf16)]
        private static partial int WriteTitle(string path, string title);
    }
}
