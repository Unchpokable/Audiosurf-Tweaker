using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace TweakerCore.DevProjects
{
    /// <summary>One project folder in a developer tab's list, before anything in it is read.</summary>
    public sealed class ProjectFolder
    {
        public string Root { get; init; }

        /// <summary>Directly inside the sources folder, as opposed to opened from elsewhere and remembered.</summary>
        public bool InSources { get; init; }
    }

    /// <summary>
    /// The project folders of a developer tab - Script Dev, Skyboxes (Docs/Internal/script-manager.md §4, §6): every
    /// project folder directly inside the sources folder (DevTools\Sources\, DevTools\SkySources\), plus folders opened
    /// from anywhere else and remembered in a recents file - the way an editor's "Open Folder" works, so the user's
    /// files stay where the user keeps them.
    ///
    /// A folder is a project when its marker file is directly in it (twproject.json, a sky's Config.json). The recents
    /// file is plain text, one full path per line, newest first, at most <see cref="RecentLimit"/>.
    /// </summary>
    public sealed class ProjectFolders
    {
        public const int RecentLimit = 20;

        private readonly string _recentsPath;
        private List<string> _recents;

        public ProjectFolders(string sourcesDirectory, string recentsPath, string markerFileName)
        {
            SourcesDirectory = sourcesDirectory;
            MarkerFileName = markerFileName;
            _recentsPath = recentsPath;
            _recents = ReadRecents(recentsPath);
        }

        public string SourcesDirectory { get; }

        public string MarkerFileName { get; }

        /// <summary>The remembered folders, newest first.</summary>
        public IReadOnlyList<string> Recent => _recents;

        public bool IsProject(string directory)
        {
            return !string.IsNullOrWhiteSpace(directory) && File.Exists(Path.Combine(directory, MarkerFileName));
        }

        /// <summary>
        /// Remembered folders first, newest first, then the rest of the sources folder by name.
        ///
        /// Reads the recents file again every time: it is a text file the user may edit, and a list that only
        /// re-read it on restart showed a hand-removed line for as long as Tweaker ran. A remembered project from
        /// the sources folder whose folder is gone is dropped from it here as well - deleting it is how one gets rid
        /// of a project there, so it is not "missing", it is deleted. One opened from elsewhere stays listed (the
        /// drive may be unplugged) until forgotten; whether it is there is the reader's to find out.
        /// </summary>
        public IReadOnlyList<ProjectFolder> List()
        {
            _recents = ReadRecents(_recentsPath);
            if (_recents.RemoveAll(root => InSources(root) && !Directory.Exists(root)) > 0)
                SaveRecents();

            var result = new List<ProjectFolder>();
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

            foreach (var root in _recents)
            {
                if (seen.Add(Normalize(root)))
                    result.Add(new ProjectFolder { Root = root, InSources = InSources(root) });
            }

            if (Directory.Exists(SourcesDirectory))
            {
                foreach (var folder in Directory.EnumerateDirectories(SourcesDirectory).OrderBy(f => f, StringComparer.OrdinalIgnoreCase))
                {
                    if (IsProject(folder) && seen.Add(Normalize(folder)))
                        result.Add(new ProjectFolder { Root = folder, InSources = true });
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

        /// <summary>A folder called <paramref name="name"/> in the sources folder.</summary>
        public string InSourcesPath(string name)
        {
            return Path.Combine(SourcesDirectory, name);
        }

        private bool InSources(string root)
        {
            var parent = Path.GetDirectoryName(Normalize(root));
            return parent != null && string.Equals(parent, Normalize(SourcesDirectory), StringComparison.OrdinalIgnoreCase);
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
