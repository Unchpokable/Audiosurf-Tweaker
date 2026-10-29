using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;

namespace TweakerCore.Skyboxes
{
    /// <summary>
    /// What makes a sky project pleasant in an editor (Docs/Internal/sky-manager-roadmap.md §5.5): VS Code with HLSL
    /// Tools (timgjones.hlsltools) should know what sky_in, sky_gradient and g_music_slow are, and not underline
    /// <c>#include "sky_common.hlsli"</c>. The plugin hands that header to a package itself, so a package must not carry
    /// it - this copy is the editor's alone:
    ///
    ///     .tweaker\include\*.hlsli     the plugin's headers, exactly as built into the DLL of this Tweaker. Tweaker's:
    ///                                   rewritten when the project is created, opened or installed, if they differ
    ///     shadertoolsconfig.json       HLSL Tools' config, pointing at .tweaker/include. The user's: written once,
    ///                                   only when there is none - it has to sit at the top, where the extension looks
    ///     .vscode\extensions.json      recommends HLSL Tools. The user's: written once, at creation, if there is none
    ///     .gitignore                   gets ".tweaker/" once - Tweaker puts the copy back, there is nothing to commit
    ///
    /// None of it goes into the package (SkyPackager leaves out dot-names and shadertoolsconfig.json).
    ///
    /// The headers are TweakerPlugin/assets/shaders/*.hlsli, embedded by TweakerCore.csproj straight from the plugin's
    /// sources, so a new header there reaches projects without a change here.
    /// </summary>
    public static class SkyEditorSupport
    {
        public const string ToolFolderName = ".tweaker";
        public const string IncludeRelativeFolder = ToolFolderName + "/include";

        private const string ResourcePrefix = "SkyHeaders/";

        internal const string ShaderToolsConfig =
            "{\n"
            + "  // HLSL Tools for VS Code (timgjones.hlsltools): where #include \"sky_common.hlsli\" is found while editing.\n"
            + "  // .tweaker/include holds a copy of the plugin's headers that Tweaker keeps up to date; the game uses its own\n"
            + "  // copy, and neither this file nor .tweaker goes into the package.\n"
            + "  \"hlsl.preprocessorDefinitions\": {},\n"
            + "  \"hlsl.additionalIncludeDirectories\": [ \"" + IncludeRelativeFolder + "\" ],\n"
            + "  \"hlsl.virtualDirectoryMappings\": {}\n"
            + "}\n";

        internal const string VsCodeExtensions =
            "{\n"
            + "  \"recommendations\": [ \"timgjones.hlsltools\" ]\n"
            + "}\n";

        private const string GitIgnoreLine = ToolFolderName + "/";

        /// <summary>The plugin's shader headers, by file name.</summary>
        public static IReadOnlyDictionary<string, byte[]> Headers { get; } = LoadHeaders();

        /// <summary>
        /// Brings the editor's files up to date - on opening a project and before installing it. Writes only what
        /// differs, so a project that is opened again sees no change at all (the folder watcher would count one as a
        /// save). Returns whether anything was written.
        /// </summary>
        public static bool Refresh(string projectRoot)
        {
            var written = false;
            var include = Path.Combine(projectRoot, ToolFolderName, "include");
            foreach (var header in Headers)
            {
                var path = Path.Combine(include, header.Key);
                if (File.Exists(path) && File.ReadAllBytes(path).AsSpan().SequenceEqual(header.Value))
                    continue;
                Directory.CreateDirectory(include);
                File.WriteAllBytes(path, header.Value);
                written = true;
            }

            var config = Path.Combine(projectRoot, SkyPackager.ShaderToolsConfigFileName);
            if (!File.Exists(config))
            {
                File.WriteAllText(config, ShaderToolsConfig, new UTF8Encoding(false));
                written = true;
            }
            return written;
        }

        /// <summary>Everything above for a project just created, plus the two files that are only offered once.</summary>
        public static void SetUpNew(string projectRoot)
        {
            Refresh(projectRoot);

            var extensions = Path.Combine(projectRoot, ".vscode", "extensions.json");
            if (!File.Exists(extensions))
            {
                Directory.CreateDirectory(Path.GetDirectoryName(extensions));
                File.WriteAllText(extensions, VsCodeExtensions, new UTF8Encoding(false));
            }

            var gitignore = Path.Combine(projectRoot, ".gitignore");
            var lines = File.Exists(gitignore) ? File.ReadAllLines(gitignore) : Array.Empty<string>();
            if (!lines.Any(line => line.Trim() == GitIgnoreLine || line.Trim() == ToolFolderName))
            {
                var text = File.Exists(gitignore) ? File.ReadAllText(gitignore) : "";
                if (text.Length > 0 && !text.EndsWith("\n", StringComparison.Ordinal))
                    text += "\n";
                File.WriteAllText(gitignore, text + GitIgnoreLine + "\n", new UTF8Encoding(false));
            }
        }

        private static IReadOnlyDictionary<string, byte[]> LoadHeaders()
        {
            var assembly = typeof(SkyEditorSupport).Assembly;
            var result = new SortedDictionary<string, byte[]>(StringComparer.OrdinalIgnoreCase);
            foreach (var resource in assembly.GetManifestResourceNames().Where(name => name.StartsWith(ResourcePrefix, StringComparison.Ordinal)))
                result[resource.Substring(ResourcePrefix.Length)] = Read(assembly, resource);
            return result;
        }

        internal static byte[] Read(Assembly assembly, string resource)
        {
            using var stream = assembly.GetManifestResourceStream(resource);
            using var memory = new MemoryStream();
            stream.CopyTo(memory);
            return memory.ToArray();
        }
    }
}
