using TagLib;

namespace QuickPlayerCore
{
    public class PlaylistRecord
    {
        public PlaylistRecord()
        {
        }

        public GenericTagsContainer Tags { get; set; }

        private string _filePath;
    }
}
