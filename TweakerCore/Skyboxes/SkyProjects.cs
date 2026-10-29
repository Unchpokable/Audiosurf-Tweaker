using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.Json;
using TweakerCore.DevProjects;

namespace TweakerCore.Skyboxes
{
    /// <summary>
    /// A sky project the Skyboxes tab lists (Docs/Internal/sky-manager-roadmap.md §3, §8.2): a folder with Config.json
    /// at its top - which is exactly a package in folder form. There is no second manifest; everything the tab knows
    /// about the project is read from Config.json.
    /// </summary>
    public sealed class SkyProject
    {
        public string Root { get; init; }

        /// <summary>The folder is gone - only a remembered one can be; the tab offers to forget it.</summary>
        public bool IsMissing { get; init; }

        /// <summary>Found in the sources folder, as opposed to opened from elsewhere and remembered.</summary>
        public bool InSources { get; init; }

        /// <summary>From Config.json; null when absent or when the manifest could not be read.</summary>
        public string Name { get; init; }
        public string Author { get; init; }

        /// <summary>The layout generation of the sky's knobs, not a release number (Docs/skyboxes/manifest.md).</summary>
        public int? Version { get; init; }

        public int LayerCount { get; init; }

        /// <summary>Why Config.json could not be read, in a line; null when it could.</summary>
        public string ManifestProblem { get; init; }

        /// <summary>Why the name will not do as a file name; null when it will (or when there is no manifest to have one).</summary>
        public string NameProblem => ManifestProblem == null ? SkyPackageName.Problem(Name) : null;

        /// <summary>"My Sky.sky" - what the sky is installed as; null without a usable name.</summary>
        public string PackageFileName => ManifestProblem == null ? SkyPackageName.FileName(Name) : null;
    }

    /// <summary>
    /// The projects of the Skyboxes tab: the folders of <see cref="ProjectFolders"/> - every project directly inside
    /// the sources folder (&lt;TweakerUI&gt;\DevTools\SkySources\) plus folders opened from anywhere else and
    /// remembered - each read for its Config.json.
    /// </summary>
    public sealed class SkyProjects
    {
        public const string ManifestFileName = "Config.json";

        private readonly ProjectFolders _folders;

        public SkyProjects(string sourcesDirectory, string recentsPath)
        {
            _folders = new ProjectFolders(sourcesDirectory, recentsPath, ManifestFileName);
        }

        public string SourcesDirectory => _folders.SourcesDirectory;

        public IReadOnlyList<string> Recent => _folders.Recent;

        public static bool IsProject(string directory)
        {
            return !string.IsNullOrWhiteSpace(directory) && File.Exists(Path.Combine(directory, ManifestFileName));
        }

        /// <summary>Remembered projects first, newest first, then the rest of the sources folder by name.</summary>
        public IReadOnlyList<SkyProject> List()
        {
            return _folders.List().Select(folder => Read(folder.Root, folder.InSources)).ToList();
        }

        /// <summary>Puts a project folder at the top of the remembered ones. False, and nothing changes, when it is not a project.</summary>
        public bool Remember(string directory) => _folders.Remember(directory);

        /// <summary>Drops a folder from the remembered ones; the folder itself is not touched.</summary>
        public void Forget(string directory) => _folders.Forget(directory);

        /// <summary>The folder a new project called <paramref name="name"/> gets in the sources folder - its package name.</summary>
        public string NewProjectPath(string name) => _folders.InSourcesPath(name);

        public static SkyProject Read(string root, bool inSources)
        {
            if (!Directory.Exists(root))
                return new SkyProject { Root = root, IsMissing = true, InSources = inSources, ManifestProblem = "the folder does not exist" };

            var manifest = Path.Combine(root, ManifestFileName);
            if (!File.Exists(manifest))
                return new SkyProject { Root = root, InSources = inSources, ManifestProblem = "no " + ManifestFileName + " in the folder" };

            try
            {
                using var document = SkyJson.Parse(File.ReadAllBytes(manifest));
                var json = document.RootElement;
                if (json.ValueKind != JsonValueKind.Object)
                    return new SkyProject { Root = root, InSources = inSources, ManifestProblem = ManifestFileName + " is not a JSON object" };

                return new SkyProject
                {
                    Root = root,
                    InSources = inSources,
                    Name = SkyJson.Text(json, SkyManifest.Name),
                    Author = SkyJson.Text(json, SkyManifest.Author),
                    Version = SkyJson.Integer(json, SkyManifest.Version),
                    LayerCount = json.TryGetProperty("layers", out var layers) && layers.ValueKind == JsonValueKind.Array ? layers.GetArrayLength() : 0,
                    ManifestProblem = SkyJson.Text(json, SkyManifest.Name) == null ? ManifestFileName + " has no \"name\" - it is the file the sky installs as" : null
                };
            }
            catch (JsonException exception)
            {
                return new SkyProject { Root = root, InSources = inSources, ManifestProblem = ManifestFileName + " is not valid JSON: " + exception.Message };
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                return new SkyProject { Root = root, InSources = inSources, ManifestProblem = "cannot read " + ManifestFileName + ": " + exception.Message };
            }
        }
    }

    /// <summary>
    /// The fields of Config.json the Skyboxes tab edits - name, author, version - written by editing the text
    /// (<see cref="JsonTextEdit"/>): a manifest is the author's document, comments and all (Docs/skyboxes/manifest.md:
    /// "A manifest is a document, not a data dump").
    /// </summary>
    public static class SkyManifest
    {
        public const string Name = "name";
        public const string Author = "author";
        public const string Version = "version";

        /// <summary>Author: free text on one line, may be empty.</summary>
        public static string AuthorProblem(string value)
        {
            return (value ?? "").Any(char.IsControl) ? "Must be a single line." : null;
        }

        /// <summary>Version: a whole number, 0 or more - the layout generation of the sky's knobs.</summary>
        public static string VersionProblem(string value)
        {
            return int.TryParse((value ?? "").Trim(), NumberStyles.None, CultureInfo.InvariantCulture, out _)
                ? null
                : "A whole number, like 1.";
        }

        /// <summary>
        /// Writes the given fields into the project's Config.json; null leaves a field as it is. Values are written as
        /// given - the problems above are the caller's to ask first.
        /// </summary>
        public static void Update(string projectRoot, string name, string author, int? version)
        {
            var changes = new List<KeyValuePair<string, string>>();
            if (name != null)
                changes.Add(new KeyValuePair<string, string>(Name, JsonTextEdit.String(name)));
            if (author != null)
                changes.Add(new KeyValuePair<string, string>(Author, JsonTextEdit.String(author)));
            if (version != null)
                changes.Add(new KeyValuePair<string, string>(Version, JsonTextEdit.Number(version.Value)));
            if (changes.Count > 0)
                JsonTextEdit.Update(Path.Combine(projectRoot, SkyProjects.ManifestFileName), changes);
        }
    }

    /// <summary>JSON the way the plugin reads it - comments and trailing commas allowed (yyjson's ALLOW_COMMENTS | ALLOW_TRAILING_COMMAS).</summary>
    internal static class SkyJson
    {
        internal static JsonDocument Parse(byte[] bytes)
        {
            return JsonDocument.Parse(bytes, new JsonDocumentOptions { CommentHandling = JsonCommentHandling.Skip, AllowTrailingCommas = true });
        }

        internal static string Text(JsonElement json, string name)
        {
            return json.TryGetProperty(name, out var value) && value.ValueKind == JsonValueKind.String ? value.GetString() : null;
        }

        internal static int? Integer(JsonElement json, string name)
        {
            return json.TryGetProperty(name, out var value) && value.ValueKind == JsonValueKind.Number && value.TryGetInt32(out var number) ? number : null;
        }
    }
}
