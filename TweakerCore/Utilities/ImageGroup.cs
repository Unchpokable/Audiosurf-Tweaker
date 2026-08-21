using System;
using System.Collections.Generic;
using System.Linq;
using SkiaSharp;
using TweakerCore.Engine;

namespace TweakerCore.Utilities
{
    public class ImageGroup : IDisposable
    {
        public string Name { get; set; }
        public IList<NamedBitmap> Group { get; private set; }

        public ImageGroup()
            : this("default")
        {
        }

        public ImageGroup(string name)
        {
            Name = name;
            Group = new List<NamedBitmap>();
        }

        public ImageGroup(string name, IEnumerable<NamedBitmap> images)
        {
            Name = name;
            Group = images.ToList();
        }

        public void AddImage(NamedBitmap image)
        {
            if (image == null)
                throw new ArgumentNullException(nameof(image), "Can't add null to image group");

            for (int i = 0; i < Group.Count; i++)
            {
                if (string.Equals(Group[i].Name, image.Name, StringComparison.OrdinalIgnoreCase))
                {
                    Group[i] = image;
                    return;
                }
            }

            Group.Add(image);
        }

        public void Apply(Action<NamedBitmap> action)
        {
            foreach (var image in Group)
                action(image);
        }

        public ImageGroup DeepClone()
        {
            return new ImageGroup(Name, Group.Where(x => x != null).Select(x => x.DeepClone()));
        }

        public static explicit operator SKBitmap(ImageGroup obj)
        {
            if (obj.Group.Count == 0)
                return null;

            if (obj.Group.Count == 1)
                return (SKBitmap)obj.Group[0];
            throw new InvalidCastException("Can't cast ImageGroup with more that 1 picture into SKBitmap");
        }

        public void Dispose()
        {
            if (Group == null)
                return;

            foreach (var image in Group)
                image.Dispose();

            Group = null;
        }
    }
}
