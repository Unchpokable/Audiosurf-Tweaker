using System;
using System.Collections.Generic;
using System.Linq;
using SkiaSharp;
using TweakerCore.Engine;
using TweakerUI.Core;

namespace TweakerUI.Models
{
    // Cache entry backing LoadingCache - screenshots are pre-rescaled once at load time so the
    // preview list never has to touch the original skin archive again.
    internal class LoadedSkinData : IDisposable
    {
        // Used by LoadingCache when reconstructing entries from the on-disk cache.
        internal LoadedSkinData()
        {
        }

        public LoadedSkinData(AudiosurfSkinExtended origin, string pathToOrigin)
        {
            var screenshots = new List<SKBitmap>();
            Name = origin.Name;
            if (origin.Previews != null)
                screenshots.AddRange(origin.Previews.Group.Select(x => ((SKBitmap)x).Rescale(860, 440)));
            Screenshots = screenshots.ToArray();
            PathToOriginFile = pathToOrigin;
        }

        public string Name { get; set; }
        public SKBitmap[] Screenshots { get; set; }
        public string PathToOriginFile { get; set; }

        public void Dispose()
        {
            // A corrupted/truncated load.cache entry can decode to a null SKBitmap (SKBitmap.Decode
            // returns null rather than throwing on bad data) - skip those instead of NRE-ing here.
            foreach (var bitmap in Screenshots ?? Enumerable.Empty<SKBitmap>())
                bitmap?.Dispose();

            Screenshots = null;
        }
    }
}
