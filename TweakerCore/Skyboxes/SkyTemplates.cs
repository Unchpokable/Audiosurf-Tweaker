using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace TweakerCore.Skyboxes
{
    /// <summary>What a new sky project can start from.</summary>
    public sealed class SkyTemplate
    {
        public SkyTemplate(string key, string title, string hint)
        {
            Key = key;
            Title = title;
            Hint = hint;
        }

        public string Key { get; }
        public string Title { get; }
        public string Hint { get; }
    }

    /// <summary>
    /// New sky projects from the templates built into TweakerCore (Docs/Internal/sky-manager-roadmap.md §5.2).
    /// Embedded rather than shipped as files, the way twllink's --new templates live in twllink: creating a project is
    /// part of the code, and a template cannot go missing from a release.
    ///
    /// Sources: TweakerCore/Skyboxes/Templates/&lt;key&gt;/. Each is a working sky - the tests check every one passes
    /// the check before packing, and the plugin's offline harness loads and compiles them.
    /// </summary>
    public static class SkyTemplates
    {
        private const string ResourcePrefix = "SkyTemplates/";

        public static IReadOnlyList<SkyTemplate> All { get; } = new[]
        {
            new SkyTemplate("gradient", "Gradient",
                "A gradient sky with a sun - the three things a manifest does: a light, bindings and knobs."),
            new SkyTemplate("music", "Music",
                "The gradient sky with a glow along the horizon that breathes with the song."),
            new SkyTemplate("clouds", "Clouds",
                "The gradient sky with a layer of clouds: the plugin draws and paints them, your script places them."),
            new SkyTemplate("minimal", "Minimal",
                "The smallest package that works, as in the getting-started guide.")
        };

        /// <summary>The files of a template, by path from the project root with '/'.</summary>
        public static IReadOnlyDictionary<string, byte[]> Files(string key)
        {
            var assembly = typeof(SkyTemplates).Assembly;
            var prefix = ResourcePrefix + key + "/";
            var result = new SortedDictionary<string, byte[]>(StringComparer.Ordinal);
            foreach (var resource in assembly.GetManifestResourceNames())
            {
                var name = resource.Replace('\\', '/');
                if (name.StartsWith(prefix, StringComparison.Ordinal))
                    result[name.Substring(prefix.Length)] = SkyEditorSupport.Read(assembly, resource);
            }
            return result;
        }

        /// <summary>
        /// Creates a project in <paramref name="directory"/>, which must not exist or be empty: the template's files,
        /// its name and author in Config.json, and the editor support (SkyEditorSupport.SetUpNew).
        /// <paramref name="author"/> may be empty. Throws IOException for a folder that has something in it.
        /// </summary>
        public static void Create(string key, string directory, string name, string author)
        {
            var files = Files(key);
            if (files.Count == 0)
                throw new ArgumentException("No sky template called " + key, nameof(key));
            if (Directory.Exists(directory) && Directory.EnumerateFileSystemEntries(directory).Any())
                throw new IOException(directory + " is not empty.");

            foreach (var file in files)
            {
                var path = Path.Combine(directory, file.Key.Replace('/', Path.DirectorySeparatorChar));
                Directory.CreateDirectory(Path.GetDirectoryName(path));
                File.WriteAllBytes(path, file.Value);
            }

            SkyManifest.Update(directory, name, author ?? "", version: null);
            SkyEditorSupport.SetUpNew(directory);
        }
    }
}
