using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using ChangerAPI.Engine;

namespace ChangerAPI.Utilities
{

    [Serializable]
    public class ImageGroup : IDisposable
    {
        public string Name { get; set; }
        public IList<NamedBitmap> Group { get; private set; }

        private bool _disposedValue;
        
        public ImageGroup()
        {
            Name = "default";
            Group = new List<NamedBitmap>();
        }

        public ImageGroup(string name, IEnumerable<NamedBitmap> images)
        {
            Name = name;
            Group = images.ToList();
        }

        public ImageGroup(string name)
        {
            Name = name;
            Group = new List<NamedBitmap>();
        }

        public ImageGroup(params Bitmap[] source)
        {
            Group = source.Select(x => (NamedBitmap)x).ToList();
        }

        public void AddImage(NamedBitmap image)
        {
            if (image == null)
                throw new ArgumentNullException($"Can't add null to image group");

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

        public void AddImage(NamedBitmap[] images)
        {
            if (images == null)
                throw new ArgumentNullException($"Can't add null to image group.");
            foreach (var image in images)
                AddImage(image);
        }

        public void Apply(Func<NamedBitmap, NamedBitmap> transform)
        {
            for (int i = 0; i < Group.Count; i++)
            {
                Group[i] = transform(Group[i]);
            }
        }

        public void Apply(Action<NamedBitmap> action)
        {
            foreach (var image in Group)
                action(image);
        }

        public void SetImageByName(string name, Bitmap newImage)
        {
            foreach (var item in Group)
            {
                if (item.Name == name)
                {
                    item.SetImage(newImage);
                    return;
                }
            }
            Group.Add(new NamedBitmap(name, newImage));
        }

        public static explicit operator Bitmap(ImageGroup obj)
        {
            if (obj.Group.Count == 1)
                return (Bitmap)obj.Group[0];
            throw new InvalidCastException("Can't cast ImageGroup with more that 1 picture into Bitmap");
        }

        public static implicit operator Bitmap[](ImageGroup obj)
        {
            return obj.Group.Select(x => (Bitmap)x).ToArray();
        }

        public static explicit operator ImageGroup(Bitmap obj)
        {
            return new ImageGroup(obj);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposedValue)
            {
                if (disposing)
                {
                    Group.ForEach(x => x.Dispose());
                }
                Group = null;
                _disposedValue = true;
            }
        }


        public void Dispose()
        {
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }
    }
}
