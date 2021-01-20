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
        public IList<Bitmap> Group { get; private set; }

        public ImageGroup(string name, IEnumerable<Bitmap> images)
        {
            Name = name;
            Group = images.ToList();
        }

        public ImageGroup(string name)
        {
            Name = name;
            Group = new List<Bitmap>();
        }

        public void AddImage(Bitmap image)
        {
            if (image == null)
                throw new NullReferenceException($"Can't add null to image group. {image}: Object reference not set to an instance of an object");
            Group.Add(image);
        }

        public static explicit operator Bitmap(ImageGroup obj)
        {
            if (obj.Group.Count == 1)
                return obj.Group[0];
            throw new InvalidCastException("Can't cast ImageGroup with more that 1 picture into Bitmap");
        }
    }
}
