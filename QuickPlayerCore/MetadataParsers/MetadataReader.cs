using System;

namespace QuickPlayerCore.MetadataParsers
{
    public class MetadataReader
    {
        public static GenericTagsContainer ReadData(string pathToFile)
        {
            if (!System.IO.File.Exists(pathToFile))
                throw new ArgumentException($"Path does not exists: {pathToFile}");

            SupportedAudioFormats.TryGetCodec(pathToFile, out var codec);

            var metadata = NativeAudioMetadata.Read(pathToFile);
            var tagsContainer = new GenericTagsContainer
            {
                SongName = metadata.Title,
                ArtistName = metadata.Artist,
                Album = metadata.Album,
                Duration = metadata.Duration,
                IsLossless = codec is PackedPresenters.Codec.Flac or PackedPresenters.Codec.Wav,
                SamplingParams = new PackedPresenters.SamplingParams
                {
                    BitDepth = (PackedPresenters.StandardBitDepth)metadata.BitDepth,
                    SamplingRate = (PackedPresenters.StandardSamplingRates)metadata.SampleRate,
                    TotalBitrate = (uint)metadata.Bitrate,
                    Codec = codec,
                }
            };

            return tagsContainer;
        }
    }
}
