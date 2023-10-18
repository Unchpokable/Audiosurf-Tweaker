using System;
using System.Collections.Generic;
using System.Linq;

namespace Updater.Core.Extensions
{
    public static class Extensions
    {
        public static bool SameWith<T>(this T source, params T[] templates)
            where T: IEquatable<T>
        {
            return templates.Any(x => x.Equals(source));
        }
    }
}