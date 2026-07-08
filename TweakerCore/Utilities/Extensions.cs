using System;
using System.Collections.Generic;
using System.IO;
using SkiaSharp;

namespace TweakerCore.Utilities
{
    public static class Extensions
    {

        public static SKBitmap Rescale(this SKBitmap source, SKSizeI newSize)
        {
            return source.Resize(newSize, SKSamplingOptions.Default);
        }


        public static void ForEach<TSource>(this IList<TSource> source, Action<TSource> action)
        {
            if (source == null)
                throw new NullReferenceException($"{nameof(source)}: Object reference not set to an instance of an object");

            if (action == null)
                throw new NullReferenceException($"{nameof(action)}: Object reference not set to an instance of an object");

            for (int i = 0; i < source.Count; i++)
            {
                action(source[i]);
            }
        }

        public static void MoveFile(string source, string target)
        {
            if (!File.Exists(source))
                throw new InvalidOperationException($"File {source} doesn't exists");
            File.Move(source, target);
        }
    }
}
