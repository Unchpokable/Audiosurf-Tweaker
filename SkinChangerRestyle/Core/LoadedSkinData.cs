using TweakerCore.Engine;
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using SkinChangerRestyle.Core.Extensions;

namespace SkinChangerRestyle.Core
{
    internal class LoadedSkinData : IDisposable
    {
        // Used by LoadingCache when reconstructing entries from the on-disk cache.
        internal LoadedSkinData()
        {
        }

        public LoadedSkinData(AudiosurfSkinExtended origin, string pathToOrigin)
        {
            var screenshots = new List<Bitmap>();
            Name = origin.Name;
            if (origin.Previews != null)
                screenshots.AddRange(origin.Previews.Group.Select(x => ((Bitmap)x).Rescale(860, 440)));
            Screenshots = screenshots.ToArray();
            PathToOriginFile = pathToOrigin;
        }

        private bool _disposedValue;

        public string Name { get; set; }
        public Bitmap[] Screenshots { get; set; }
        public string PathToOriginFile { get; set; }

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposedValue)
            {
                if (disposing)
                {
                    foreach (var bmp in Screenshots)
                        bmp.Dispose();
                }
                Screenshots = null;
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
