using System;
using System.Collections.Generic;
using System.Linq;

namespace Updater.Core.Extensions
{
    public static class Extensions
    {
        public static T? FirstOrNull<T>(this IEnumerable<T> source, Predicate<T> predicate)
            where T : class
        {
            foreach (var item in source)
            {
                if (predicate(item))
                    return item;
            }

            return null;
        }

        public static T? FirstOfNull<T>(this IEnumerable<T> source, Predicate<T> predicate) 
            where T : struct
        {
            foreach (var item in source)
            {
                if (predicate(item))
                    return item;
            }

            return null;
        }

        public static bool SameWith<T>(this T source, params T[] templates)
            where T: IEquatable<T>
        {
            return templates.Any(x => x.Equals(source));
        }
    }
}