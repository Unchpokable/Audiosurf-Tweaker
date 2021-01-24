﻿namespace Audiosurf_SkinChanger.Utilities
{
    using System;
    using System.Collections.Generic;
    using System.Drawing;
    using System.Linq;
    using Audiosurf_SkinChanger.Engine;

    [Serializable]
    class ImageGroup
    {
        public string Name { get; set; }
        public IList<NamedBitmap> Group { get; private set; }

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

        public void AddImage(NamedBitmap image)
        {
            if (image == null)
                throw new NullReferenceException($"Can't add null to image group. {image}: Object reference not set to an instance of an object");
            Group.Add(image);
        }

        public void AddImage(NamedBitmap[] images)
        {
            if (images == null)
                throw new NullReferenceException($"Can't add null to image group. {images}: Object reference not set to an instance of an object");
            foreach (var bitmap in images)
                Group.Add(bitmap);
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
    }
}