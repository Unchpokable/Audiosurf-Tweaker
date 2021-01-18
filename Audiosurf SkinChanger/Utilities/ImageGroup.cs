namespace Audiosurf_SkinChanger.Utilities
{
    using System;
    using System.Collections.Generic;
    using System.Drawing;
    using System.Linq;

    [Serializable]
    class ImageGroup
    {
        public string Name { get; set; }
        public IList<Bitmap> Group { get; set; }

        public ImageGroup(string name, IEnumerable<Bitmap> images)
        {
            Name = name;
            Group = images.ToList();
        }

        public ImageGroup(string name)
        {
            Name = name;
        }

        public void AddImage(Bitmap image)
        {
            if (image == null)
                throw new NullReferenceException($"Can't add null to image group. {image}: Object reference not set to an instance of an object");
            Group.Add(image);
        }
    }
}
