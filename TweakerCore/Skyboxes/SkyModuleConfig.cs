using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.Json;
using TweakerCore.DevProjects;

namespace TweakerCore.Skyboxes
{
    /// <summary>
    /// What kind of thing a sky selection names - the spelling of module.json and of the SKY_* protocol, which is the
    /// plugin's own (skybox_config.cxx: kind_key). Declaration order is the plugin catalog's list order.
    /// </summary>
    public enum SkyKind
    {
        /// <summary>A shader built into the plugin; the id is its program id ("gradient").</summary>
        Program,

        /// <summary>A loose .hlsl in the Skyboxes folder.</summary>
        ShaderFile,

        /// <summary>An image built into the plugin; the id is its resource key ("skyboxes/cloudy_01.png").</summary>
        Packed,

        /// <summary>A cross or panorama image in the Skyboxes folder.</summary>
        File,

        /// <summary>A folder of six faces in the Skyboxes folder.</summary>
        FaceDir,

        /// <summary>A .sky package - a folder with Config.json, or its zip.</summary>
        Package
    }

    public static class SkyKinds
    {
        private static readonly Dictionary<SkyKind, string> Keys = new Dictionary<SkyKind, string>
        {
            [SkyKind.Program] = "program",
            [SkyKind.ShaderFile] = "shader_file",
            [SkyKind.Packed] = "packed",
            [SkyKind.File] = "file",
            [SkyKind.FaceDir] = "face_dir",
            [SkyKind.Package] = "package"
        };

        public static string Key(SkyKind kind) => Keys[kind];

        public static bool TryParse(string key, out SkyKind kind)
        {
            foreach (var pair in Keys)
            {
                if (pair.Value == key)
                {
                    kind = pair.Key;
                    return true;
                }
            }
            kind = default;
            return false;
        }

        /// <summary>The kinds whose id is a name in the Skyboxes folder - compared without case, like the file system.</summary>
        public static bool IsOnDisk(SkyKind kind) => kind != SkyKind.Program && kind != SkyKind.Packed;
    }

    /// <summary>A sky: what kind of thing, and its id. Equal when the kind matches and the id does - without case for one on disk.</summary>
    public sealed record SkySelection(SkyKind Kind, string Id)
    {
        public bool Matches(SkyKind kind, string id)
        {
            return Kind == kind && string.Equals(Id, id, SkyKinds.IsOnDisk(kind) ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal);
        }
    }

    /// <summary>
    /// The Skybox Replacer's module.json without the game (Docs/Internal/sky-manager-roadmap.md §5.6, §8.7): whether
    /// the replacer is on and which sky it draws. Only those two - the rest of the file (cube map, orientation, shader
    /// quality) is the overlay's and stays byte for byte.
    ///
    /// Read the way the plugin reads it (skybox_config.cxx): comments and trailing commas allowed, unknown keys
    /// ignored, a "sky" without a known string "kind" and a non-empty string "id" means the default sky. While the
    /// game runs the plugin owns this file - it reads it at startup and rewrites it on every change - so writing it is
    /// for when the game is not running, which the caller checks.
    ///
    /// A file that is there but cannot be read is never written: the plugin's rule is that a typo costs nothing the
    /// player had, and a host that "fixed" the file would cost exactly that.
    /// </summary>
    public sealed class SkyModuleConfig
    {
        /// <summary>What a fresh module.json selects (skybox_config.hxx: k_default_packed).</summary>
        public static readonly SkySelection DefaultSky = new SkySelection(SkyKind.Packed, "skyboxes/cloudy_01.png");

        public bool Enabled { get; private set; } = true;

        public SkySelection Sky { get; private set; } = DefaultSky;

        /// <summary>The file is there. Missing means the plugin has not run yet; the defaults above stand.</summary>
        public bool Exists { get; private set; }

        /// <summary>Why the file could not be read, when it is there and could not be; null otherwise. Such a file is not written.</summary>
        public string Problem { get; private set; }

        public static SkyModuleConfig Load(string engineDirectory)
        {
            var config = new SkyModuleConfig();
            var path = SkyPaths.ModuleConfigPath(engineDirectory);
            if (!File.Exists(path))
                return config;

            config.Exists = true;
            try
            {
                using var document = SkyJson.Parse(File.ReadAllBytes(path));
                var root = document.RootElement;
                if (root.ValueKind != JsonValueKind.Object)
                {
                    config.Problem = SkyPaths.ModuleConfigFileName + " is not a JSON object";
                    return config;
                }

                if (root.TryGetProperty("enabled", out var enabled) && (enabled.ValueKind == JsonValueKind.True || enabled.ValueKind == JsonValueKind.False))
                    config.Enabled = enabled.GetBoolean();

                if (root.TryGetProperty("sky", out var sky) && sky.ValueKind == JsonValueKind.Object)
                {
                    var kind = SkyJson.Text(sky, "kind");
                    var id = SkyJson.Text(sky, "id");
                    if (kind != null && !string.IsNullOrEmpty(id) && SkyKinds.TryParse(kind, out var parsed))
                        config.Sky = new SkySelection(parsed, id);
                }
            }
            catch (JsonException exception)
            {
                config.Problem = SkyPaths.ModuleConfigFileName + " is not valid JSON: " + exception.Message;
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                config.Problem = "cannot read " + SkyPaths.ModuleConfigFileName + ": " + exception.Message;
            }
            return config;
        }

        /// <summary>
        /// Switches the replacer on or off and/or selects a sky; null leaves that one as it is. A missing file is
        /// created with just these two (the plugin fills in the rest with its defaults); an existing one is edited in
        /// place. Throws <see cref="InvalidDataException"/> for a file that is there and cannot be read - it is left
        /// alone - and IOException for a file that cannot be written.
        /// </summary>
        public static void Write(string engineDirectory, bool? enabled, SkySelection sky)
        {
            var current = Load(engineDirectory);
            if (current.Problem != null)
                throw new InvalidDataException(current.Problem + " - Tweaker leaves it as it is; fix or delete it.");

            var path = SkyPaths.ModuleConfigPath(engineDirectory);
            if (!current.Exists)
            {
                Directory.CreateDirectory(Path.GetDirectoryName(path));
                var text = "{\n"
                           + "  \"enabled\": " + JsonTextEdit.Bool(enabled ?? current.Enabled) + ",\n"
                           + "  \"sky\": " + SelectionJson(sky ?? current.Sky) + "\n"
                           + "}\n";
                File.WriteAllText(path, text, new UTF8Encoding(false));
                return;
            }

            var changes = new List<KeyValuePair<string, string>>();
            if (enabled != null)
                changes.Add(new KeyValuePair<string, string>("enabled", JsonTextEdit.Bool(enabled.Value)));
            if (sky != null)
                changes.Add(new KeyValuePair<string, string>("sky", SelectionJson(sky)));
            if (changes.Count > 0)
                JsonTextEdit.Update(path, changes);
        }

        /// <summary>The "sky" object as the plugin writes it: <c>{ "kind": "package", "id": "My Sky.sky" }</c>.</summary>
        private static string SelectionJson(SkySelection sky)
        {
            return "{ \"kind\": " + JsonTextEdit.String(SkyKinds.Key(sky.Kind)) + ", \"id\": " + JsonTextEdit.String(sky.Id) + " }";
        }
    }
}
