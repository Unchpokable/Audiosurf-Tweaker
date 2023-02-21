using QuickPlayerCore.PackedPresenters;
using System;
using System.Linq;
using TagLib;

namespace QuickPlayerCore.MetadataParsers
{
    public class MetadataReader
    {
        public static GenericTagsContainer ReadData(string pathToFile)
        {
            if (!System.IO.File.Exists(pathToFile))
                throw new ArgumentException($"Path does not exists: {pathToFile}");

            var file = File.Create(pathToFile);
            var tagsContainer = new GenericTagsContainer();
            tagsContainer.SongName = file.Tag.Title;
            tagsContainer.ArtistName = file.Tag.FirstPerformer;
            tagsContainer.Album = file.Tag.Album;
            tagsContainer.Duration = file.Properties.Duration;
            tagsContainer.SamplingParams = new SamplingParams
            {
                BitDepth = (StandardBitDepth)file.Properties.BitsPerSample,
                SamplingRate = (StandardSamplingRates)file.Properties.AudioSampleRate,
                TotalBitrate = (uint)file.Properties.AudioBitrate,
            };

            if (new[] { ".flac", ".m4a", ".wav", ".wave" }.Any(x => System.IO.Path.GetExtension(pathToFile) == x))
                tagsContainer.IsLossless = true;

            if (Enum.TryParse(System.IO.Path.GetExtension(pathToFile), out Codec codec))
                tagsContainer.SamplingParams.Codec = codec;
            else
                tagsContainer.SamplingParams.Codec = Codec.Unsupported;
            return tagsContainer;
        }
    }
}
