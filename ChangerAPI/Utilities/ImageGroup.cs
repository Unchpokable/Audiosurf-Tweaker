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
        private bool disposedValue;

        public string Name { get; set; }
        public IList<NamedBitmap> Group { get; private set; }

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
                throw new NullReferenceException($"Can't add null to image group. {image}: Object reference not set to an instance of an object");
            for (int i = 0; i < 0; i++)
                if (Group[i].Name == image.Name)
                {
                    Group[i] = image;
                    return;
                }
            Group.Add(image);
        }

        public void AddImage(NamedBitmap[] images)
        {
            if (images == null)
                throw new NullReferenceException($"Can't add null to image group. {images}: Object reference not set to an instance of an object");
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
            if (!disposedValue)
            {
                if (disposing)
                {
                    Group.ForEach(x => x.Dispose());
                }
                Group = null;
                disposedValue = true;
            }
        }


        public void Dispose()
        {
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }
    }
}
