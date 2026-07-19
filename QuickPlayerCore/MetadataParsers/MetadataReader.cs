using System;
using TagLib;

namespace QuickPlayerCore.MetadataParsers
{
    public class MetadataReader
    {
        public static GenericTagsContainer ReadData(string pathToFile)
        {
            if (!System.IO.File.Exists(pathToFile))
                throw new ArgumentException($"Path does not exists: {pathToFile}");

            SupportedAudioFormats.TryGetCodec(pathToFile, out var codec);

            var file = TagLib.File.Create(pathToFile);
            var tagsContainer = new GenericTagsContainer
            {
                SongName = file.Tag.Title,
                ArtistName = file.Tag.FirstPerformer,
                Album = file.Tag.Album,
                Duration = file.Properties.Duration,
                IsLossless = codec is PackedPresenters.Codec.Flac or PackedPresenters.Codec.Wav,
                SamplingParams = new PackedPresenters.SamplingParams
                {
                    BitDepth = (PackedPresenters.StandardBitDepth)file.Properties.BitsPerSample,
                    SamplingRate = (PackedPresenters.StandardSamplingRates)file.Properties.AudioSampleRate,
                    TotalBitrate = (uint)file.Properties.AudioBitrate,
                    Codec = codec,
                }
            };

            return tagsContainer;
        }
    }
}
