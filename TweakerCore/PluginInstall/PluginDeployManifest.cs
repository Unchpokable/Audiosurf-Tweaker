using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace TweakerCore.PluginInstall
{
    /// <summary>
    /// One entry of the deployment manifest: the hash of the file <b>as the host wrote it</b>.
    /// </summary>
    public sealed class PluginDeployFile
    {
        [JsonPropertyName("sha256")]
        public string Sha256 { get; set; }
    }

    /// <summary>
    /// engine\TweakerStuff\.deploy.json - the record of what Tweaker itself put into the game folder, and of
    /// the content it put there (Docs/Internal/plugin-offline-mode.md §6.2).
    ///
    /// It is deliberately not an inventory of the folder: a file the user added or a skybox they downloaded is
    /// theirs, never appears here, and is never touched. The hash is what makes "the user edited this shipped
    /// script" distinguishable from "this shipped script is still ours to overwrite".
    /// </summary>
    public sealed class PluginDeployManifest
    {
        public const string FileName = ".deploy.json";

        [JsonPropertyName("plugin_version")]
        public string PluginVersion { get; set; }

        /// <summary>
        /// Keyed by path relative to the engine folder, with '/' separators ("channels/TweakerPlugin.dll").
        /// Compared case-insensitively, like the file system underneath it.
        /// </summary>
        [JsonPropertyName("files")]
        public Dictionary<string, PluginDeployFile> Files { get; set; }
            = new Dictionary<string, PluginDeployFile>(StringComparer.OrdinalIgnoreCase);

        /// <summary>Whether a manifest file was actually read - false for the empty one Load() invents.</summary>
        [JsonIgnore]
        public bool Exists { get; private set; }

        public static string PathFor(string engineDirectory)
        {
            return Path.Combine(engineDirectory, PluginInstallation.StuffFolderName, FileName);
        }

        /// <summary>
        /// Missing or unreadable manifest reads as an empty one: every file on disk then counts as the user's,
        /// which is the safe direction - the worst case is a backup copy nobody needed, not a lost edit.
        /// </summary>
        public static PluginDeployManifest Load(string engineDirectory)
        {
            var path = PathFor(engineDirectory);
            if (!File.Exists(path))
                return new PluginDeployManifest();

            PluginDeployManifest loaded = null;
            try
            {
                loaded = JsonSerializer.Deserialize<PluginDeployManifest>(File.ReadAllText(path));
            }
            catch (JsonException)
            {
            }
            catch (IOException)
            {
            }

            if (loaded == null)
                return new PluginDeployManifest();

            // Deserialization builds an ordinal dictionary of its own, so the comparer has to be re-applied.
            loaded.Files = loaded.Files == null
                ? new Dictionary<string, PluginDeployFile>(StringComparer.OrdinalIgnoreCase)
                : new Dictionary<string, PluginDeployFile>(loaded.Files, StringComparer.OrdinalIgnoreCase);
            loaded.Exists = true;

            return loaded;
        }

        public void Save(string engineDirectory)
        {
            var path = PathFor(engineDirectory);
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            File.WriteAllText(path, JsonSerializer.Serialize(this, new JsonSerializerOptions { WriteIndented = true }));
            Exists = true;
        }

        public bool TryGetHash(string relativePath, out string sha256)
        {
            sha256 = null;
            if (Files == null || !Files.TryGetValue(relativePath, out var entry) || entry == null)
                return false;

            sha256 = entry.Sha256;
            return sha256 != null;
        }

        public void Set(string relativePath, string sha256)
        {
            Files[relativePath] = new PluginDeployFile { Sha256 = sha256 };
        }

        public void Remove(string relativePath)
        {
            Files.Remove(relativePath);
        }
    }
}
