using System;
using System.Collections.Generic;
using System.Linq;

namespace TweakerCore.Scripting
{
    /// <summary>
    /// The run order of scripts (Docs/Internal/script-coexistence.md §3) as lists of file names, top first.
    /// The order is the user's and nobody else's: the plugin keeps it in scripts.cfg as <c>order=</c> lines and runs
    /// every list of callbacks by it; the Scripts tab shows it and changes it by drag.
    /// </summary>
    public static class ScriptOrder
    {
        /// <summary>
        /// The order of <paramref name="fileNames"/> the way the plugin builds it at startup (lua_registry,
        /// initial_order): the ones <paramref name="order"/> lists, in its order, then the rest by file name, ordinal.
        /// Names in the order that are not among the files are skipped.
        /// </summary>
        public static IReadOnlyList<string> Arrange(IEnumerable<string> fileNames, IReadOnlyList<string> order)
        {
            var files = new HashSet<string>(fileNames, StringComparer.Ordinal);
            var result = new List<string>(files.Count);

            foreach (var name in order ?? Array.Empty<string>())
            {
                if (files.Remove(name))
                    result.Add(name);
            }

            result.AddRange(files.OrderBy(name => name, StringComparer.Ordinal));
            return result;
        }

        /// <summary>
        /// <paramref name="order"/> with <paramref name="fileName"/> taken out and put back right before
        /// <paramref name="before"/> - at the end when that is null or not in the order. Every other name keeps its
        /// place relative to the rest, including names the caller does not show: the plugin's order can hold a
        /// script whose file went away during the session, and a drag elsewhere must not move it.
        /// </summary>
        public static IReadOnlyList<string> Move(IReadOnlyList<string> order, string fileName, string before)
        {
            var result = order.Where(name => !string.Equals(name, fileName, StringComparison.Ordinal)).ToList();
            var at = before == null ? -1 : result.FindIndex(name => string.Equals(name, before, StringComparison.Ordinal));
            result.Insert(at < 0 ? result.Count : at, fileName);
            return result;
        }
    }
}
