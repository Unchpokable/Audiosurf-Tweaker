using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;

namespace TweakerCore.Scripting
{
    /// <summary>A twllink project the Script Dev tab lists.</summary>
    public sealed class ScriptProject
    {
        /// <summary>The project folder: the one holding twproject.json.</summary>
        public string Root { get; init; }

        /// <summary>The folder is gone - only a recent entry can be; the tab offers to forget it.</summary>
        public bool IsMissing { get; init; }

        /// <summary>Found in the sources folder, as opposed to opened from elsewhere and remembered.</summary>
        public bool InSources { get; init; }

        /// <summary>From twproject.json; null when it could not be read - <see cref="ManifestProblem"/> says why.</summary>
        public string Id { get; init; }
        public string Name { get; init; }
        public string Version { get; init; }
        public string Description { get; init; }
        public string Author { get; init; }

        /// <summary>
        /// Why the manifest could not be read, in a line; null when it could. Only enough to list the project:
        /// what is actually wrong with it is twllink's to say, and a build says it.
        /// </summary>
        public string ManifestProblem { get; init; }

        /// <summary>"my_hud.lua" - the file name the script gets in the game; null without an id.</summary>
        public string ScriptFileName => Id == null ? null : Id + GameScripts.ScriptExtension;
    }

    /// <summary>
    /// The projects of the Script Dev tab (Docs/Internal/script-manager-roadmap.md §5, п. 3): every project
    /// folder directly inside the sources folder (&lt;TweakerUI&gt;\DevTools\Sources\ by default), plus
    /// folders opened from anywhere else and remembered in a recents file - the way an editor's "Open Folder"
    /// works, so the user's files stay where the user keeps them.
    ///
    /// A folder is a project when twproject.json is in it; its name is fixed (twllink's
    /// <c>Manifest.FileName</c>). The recents file is plain text, one full path per line, newest first.
    /// </summary>
    public sealed class ScriptProjects
    {
        public const string ManifestFileName = "twproject.json";
        public const int RecentLimit = 20;

        private readonly string _recentsPath;
        private List<string> _recents;

        public ScriptProjects(string sourcesDirectory, string recentsPath)
        {
            SourcesDirectory = sourcesDirectory;
            _recentsPath = recentsPath;
            _recents = ReadRecents(recentsPath);
        }

        public string SourcesDirectory { get; }

        /// <summary>The remembered folders, newest first.</summary>
        public IReadOnlyList<string> Recent => _recents;

        public static bool IsProject(string directory)
        {
            return !string.IsNullOrWhiteSpace(directory) && File.Exists(Path.Combine(directory, ManifestFileName));
        }

        /// <summary>
        /// Remembered projects first, newest first, then the rest of the sources folder by name.
        ///
        /// Reads the recents file again every time: it is a text file the user may edit, and a list that only
        /// re-read it on restart showed a hand-removed line for as long as Tweaker ran. A remembered project from
        /// the sources folder whose folder is gone is dropped from it here as well - deleting it is how one gets rid
        /// of a project there, so it is not "missing", it is deleted. One opened from elsewhere stays listed as
        /// missing (the drive may be unplugged) until forgotten.
        /// </summary>
        public IReadOnlyList<ScriptProject> List()
        {
            _recents = ReadRecents(_recentsPath);
            if (_recents.RemoveAll(root => InSources(root) && !Directory.Exists(root)) > 0)
                SaveRecents();

            var result = new List<ScriptProject>();
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

            foreach (var root in _recents)
            {
                if (seen.Add(Normalize(root)))
                    result.Add(Read(root, InSources(root)));
            }

            if (Directory.Exists(SourcesDirectory))
            {
                foreach (var folder in Directory.EnumerateDirectories(SourcesDirectory).OrderBy(f => f, StringComparer.OrdinalIgnoreCase))
                {
                    if (IsProject(folder) && seen.Add(Normalize(folder)))
                        result.Add(Read(folder, inSources: true));
                }
            }

            return result;
        }

        /// <summary>
        /// Puts a project folder at the top of the recents and saves them. False, and nothing changes, when the
        /// folder is not a project.
        /// </summary>
        public bool Remember(string directory)
        {
            if (!IsProject(directory))
                return false;

            _recents = ReadRecents(_recentsPath);
            var full = Normalize(directory);
            _recents.RemoveAll(path => string.Equals(Normalize(path), full, StringComparison.OrdinalIgnoreCase));
            _recents.Insert(0, full);
            if (_recents.Count > RecentLimit)
                _recents.RemoveRange(RecentLimit, _recents.Count - RecentLimit);
            SaveRecents();
            return true;
        }

        /// <summary>Drops a folder from the recents; the folder itself is not touched.</summary>
        public void Forget(string directory)
        {
            _recents = ReadRecents(_recentsPath);
            var full = Normalize(directory);
            if (_recents.RemoveAll(path => string.Equals(Normalize(path), full, StringComparison.OrdinalIgnoreCase)) > 0)
                SaveRecents();
        }

        /// <summary>
        /// The folder a new project called <paramref name="id"/> would get in the sources folder. twllink derives
        /// the project's id from the folder name, so the name is the id.
        /// </summary>
        public string NewProjectPath(string id)
        {
            return Path.Combine(SourcesDirectory, id);
        }

        public static ScriptProject Read(string root, bool inSources)
        {
            if (!Directory.Exists(root))
                return new ScriptProject { Root = root, IsMissing = true, InSources = inSources, ManifestProblem = "the folder does not exist" };

            var manifest = Path.Combine(root, ManifestFileName);
            if (!File.Exists(manifest))
                return new ScriptProject { Root = root, InSources = inSources, ManifestProblem = "no " + ManifestFileName + " in the folder" };

            try
            {
                using var document = JsonDocument.Parse(File.ReadAllBytes(manifest),
                    new JsonDocumentOptions { CommentHandling = JsonCommentHandling.Skip, AllowTrailingCommas = true });
                var json = document.RootElement;
                if (json.ValueKind != JsonValueKind.Object)
                    return new ScriptProject { Root = root, InSources = inSources, ManifestProblem = ManifestFileName + " is not a JSON object" };

                return new ScriptProject
                {
                    Root = root,
                    InSources = inSources,
                    Id = Text(json, "id"),
                    Name = Text(json, "name"),
                    Version = Text(json, "version"),
                    Description = Text(json, "description"),
                    Author = Text(json, "author"),
                    ManifestProblem = Text(json, "id") == null ? ManifestFileName + " has no \"id\"" : null
                };
            }
            catch (JsonException exception)
            {
                return new ScriptProject { Root = root, InSources = inSources, ManifestProblem = ManifestFileName + " is not valid JSON: " + exception.Message };
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                return new ScriptProject { Root = root, InSources = inSources, ManifestProblem = "cannot read " + ManifestFileName + ": " + exception.Message };
            }
        }

        private bool InSources(string root)
        {
            var parent = Path.GetDirectoryName(Normalize(root));
            return parent != null && string.Equals(parent, Normalize(SourcesDirectory), StringComparison.OrdinalIgnoreCase);
        }

        private static string Text(JsonElement json, string name)
        {
            return json.TryGetProperty(name, out var value) && value.ValueKind == JsonValueKind.String ? value.GetString() : null;
        }

        private static string Normalize(string path)
        {
            return Path.TrimEndingDirectorySeparator(Path.GetFullPath(path));
        }

        private static List<string> ReadRecents(string path)
        {
            try
            {
                if (File.Exists(path))
                {
                    return File.ReadAllLines(path, new UTF8Encoding(false))
                        .Select(line => line.Trim())
                        .Where(line => line.Length > 0 && Path.IsPathFullyQualified(line))
                        .Take(RecentLimit)
                        .ToList();
                }
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
            }
            return new List<string>();
        }

        private void SaveRecents()
        {
            Directory.CreateDirectory(Path.GetDirectoryName(_recentsPath));
            File.WriteAllLines(_recentsPath, _recents, new UTF8Encoding(false));
        }
    }
}
