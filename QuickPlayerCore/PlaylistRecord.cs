using QuickPlayerCore.MetadataParsers;
using System.IO;

namespace QuickPlayerCore
{
    public class PlaylistRecord
    {
        public PlaylistRecord(string origin)
        {
            if (!File.Exists(origin))
            {
                throw new FileNotFoundException($"File {origin} does not exists");
            }            

            _filePath = origin;
            Tags = MetadataReader.ReadData(_filePath);
        }

        public GenericTagsContainer Tags { get; set; }

        private string _filePath;

        public override string ToString()
        {
            if (Tags.IsLossless)
                return $"{Tags.ArtistName} - {Tags.SongName}\n{Tags.SamplingParams.Codec} at {(int)Tags.SamplingParams.SamplingRate} kHz {(int)Tags.SamplingParams.BitDepth} bit :: {(int)Tags.SamplingParams.TotalBitrate}";
            
            return $"{Tags.ArtistName} - {Tags.SongName}\n{Tags.SamplingParams.Codec} at {Tags.SamplingParams.TotalBitrate}";
            
        }
    }
}
