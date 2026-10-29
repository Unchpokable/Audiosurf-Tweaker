using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using TweakerCore.DevProjects;

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
    /// The projects of the Script Dev tab (Docs/Internal/script-manager.md §8.1): the folders of
    /// <see cref="ProjectFolders"/> - every project directly inside the sources folder (&lt;TweakerUI&gt;\DevTools\Sources\
    /// by default) plus folders opened from anywhere else and remembered - each read for its twproject.json.
    ///
    /// A folder is a project when twproject.json is in it; its name is fixed (twllink's <c>Manifest.FileName</c>).
    /// </summary>
    public sealed class ScriptProjects
    {
        public const string ManifestFileName = "twproject.json";
        public const int RecentLimit = ProjectFolders.RecentLimit;

        private readonly ProjectFolders _folders;

        public ScriptProjects(string sourcesDirectory, string recentsPath)
        {
            _folders = new ProjectFolders(sourcesDirectory, recentsPath, ManifestFileName);
        }

        public string SourcesDirectory => _folders.SourcesDirectory;

        /// <summary>The remembered folders, newest first.</summary>
        public IReadOnlyList<string> Recent => _folders.Recent;

        public static bool IsProject(string directory)
        {
            return !string.IsNullOrWhiteSpace(directory) && File.Exists(Path.Combine(directory, ManifestFileName));
        }

        /// <summary>Remembered projects first, newest first, then the rest of the sources folder by name (<see cref="ProjectFolders.List"/>).</summary>
        public IReadOnlyList<ScriptProject> List()
        {
            return _folders.List().Select(folder => Read(folder.Root, folder.InSources)).ToList();
        }

        /// <summary>
        /// Puts a project folder at the top of the recents and saves them. False, and nothing changes, when the
        /// folder is not a project.
        /// </summary>
        public bool Remember(string directory) => _folders.Remember(directory);

        /// <summary>Drops a folder from the recents; the folder itself is not touched.</summary>
        public void Forget(string directory) => _folders.Forget(directory);

        /// <summary>
        /// The folder a new project called <paramref name="id"/> would get in the sources folder. twllink derives
        /// the project's id from the folder name, so the name is the id.
        /// </summary>
        public string NewProjectPath(string id) => _folders.InSourcesPath(id);

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

        private static string Text(JsonElement json, string name)
        {
            return json.TryGetProperty(name, out var value) && value.ValueKind == JsonValueKind.String ? value.GetString() : null;
        }
    }
}
