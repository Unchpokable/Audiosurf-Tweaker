using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text.Json;
using System.Threading;

namespace TweakerCore.Skyboxes
{
    public enum SkyProblemSeverity
    {
        Warning,
        Error
    }

    /// <summary>One thing the check before packing found.</summary>
    public sealed class SkyProblem
    {
        public SkyProblemSeverity Severity { get; init; }

        /// <summary>The file it is about, from the project root with '/'; Config.json for anything in the manifest.</summary>
        public string File { get; init; }

        public string Message { get; init; }

        public bool IsError => Severity == SkyProblemSeverity.Error;

        public override string ToString() => (IsError ? "error" : "warning") + ": " + File + ": " + Message;
    }

    public enum SkyInstallOutcome
    {
        /// <summary>Written. There may still be warnings.</summary>
        Installed,

        /// <summary>The check found errors; nothing was written and the game keeps what it had.</summary>
        Problems,

        /// <summary>Something in the way that is not ours to move - a folder of the same name, no plugin folder.</summary>
        Refused,

        /// <summary>Reading or writing failed.</summary>
        Failed
    }

    public sealed class SkyInstallResult
    {
        public SkyInstallOutcome Outcome { get; init; }

        public IReadOnlyList<SkyProblem> Problems { get; init; } = Array.Empty<SkyProblem>();

        /// <summary>For Refused and Failed: why, in a sentence.</summary>
        public string Message { get; init; }

        /// <summary>The written file; null unless Installed.</summary>
        public string Path { get; init; }

        /// <summary>The written file's name ("My Sky.sky") - the sky's id in the game; null unless Installed.</summary>
        public string FileName => Path == null ? null : System.IO.Path.GetFileName(Path);

        public long Size { get; init; }

        public bool Succeeded => Outcome == SkyInstallOutcome.Installed;
    }

    /// <summary>
    /// A sky project into a .sky (Docs/Internal/sky-manager-roadmap.md §5.3): check, zip, put into the game.
    ///
    /// **What goes in the zip** is the whole project but what is not the sky's: anything whose name starts with a dot
    /// (.git, .vscode, .tweaker - the editor's copy of sky_common.hlsli lives there), shadertoolsconfig.json at the
    /// top (the editor's), and the OS's own litter. Config.json sits at the top of the archive.
    ///
    /// **The check** is only what is cheap and certain - the manifest parses, it has layers, and every file it names
    /// is in the package and is a name the plugin would agree to read. Everything else about the format is the
    /// plugin's to say (sky_package): a second copy of its rules here would drift from the first at the next change of
    /// the format. A check with errors installs nothing, and the game keeps what it had.
    ///
    /// **Writing into the game is atomic**: the zip is written beside the target under a name the plugin's catalog
    /// does not list, then moved over it. The plugin watches the archive's write time for hot reload and would
    /// otherwise pick up a half-written one; it reads an archive into memory when it opens it and does not hold the
    /// file, but it may be reading at the moment of the move - hence a few retries.
    /// </summary>
    public static class SkyPackager
    {
        /// <summary>
        /// Above this the archive is worth a warning. The plugin reads it into memory whole, in a 32-bit game without
        /// LARGEADDRESSAWARE; 64 MB packed is already the point to look at how many images there are and how big
        /// (owner's decision, sky-manager-roadmap.md §9).
        /// </summary>
        public const long SizeWarningBytes = 64L * 1024 * 1024;

        /// <summary>The editor's config for HLSL Tools - in the project's root because that is where the extension looks.</summary>
        public const string ShaderToolsConfigFileName = "shadertoolsconfig.json";

        private static readonly string[] OsLitter = { "Thumbs.db", "desktop.ini", ".DS_Store" };

        private static readonly char[] ForbiddenInComponent = { ':', '*', '?', '"', '<', '>', '|' };

        private static readonly string[] Devices = { "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };

        /// <summary>The files that go into the package, from the root with '/', in a stable order.</summary>
        public static IReadOnlyList<string> PackageFiles(string projectRoot)
        {
            var root = Path.GetFullPath(projectRoot);
            var result = new List<string>();
            Collect(root, root, result);
            result.Sort(StringComparer.OrdinalIgnoreCase);
            return result;
        }

        private static void Collect(string root, string directory, List<string> result)
        {
            foreach (var file in Directory.EnumerateFiles(directory))
            {
                var name = Path.GetFileName(file);
                if (name.StartsWith(".", StringComparison.Ordinal) || OsLitter.Contains(name, StringComparer.OrdinalIgnoreCase))
                    continue;
                if (directory == root && string.Equals(name, ShaderToolsConfigFileName, StringComparison.OrdinalIgnoreCase))
                    continue;
                result.Add(Path.GetRelativePath(root, file).Replace('\\', '/'));
            }
            foreach (var sub in Directory.EnumerateDirectories(directory))
            {
                if (!Path.GetFileName(sub).StartsWith(".", StringComparison.Ordinal))
                    Collect(root, sub, result);
            }
        }

        /// <summary>What is wrong with the project as a package; empty when nothing is.</summary>
        public static IReadOnlyList<SkyProblem> Check(string projectRoot)
        {
            var problems = new List<SkyProblem>();
            const string manifestName = SkyProjects.ManifestFileName;
            void Error(string file, string message) => problems.Add(new SkyProblem { Severity = SkyProblemSeverity.Error, File = file, Message = message });
            void Warning(string file, string message) => problems.Add(new SkyProblem { Severity = SkyProblemSeverity.Warning, File = file, Message = message });

            var manifestPath = Path.Combine(projectRoot, manifestName);
            if (!File.Exists(manifestPath))
            {
                Error(manifestName, "there is no Config.json at the top of the project - it is the one file a package must have");
                return problems;
            }

            var name = SkyProjects.Read(projectRoot, inSources: false).Name;
            var nameProblem = name == null ? "Config.json has no \"name\" - it is the file the sky installs as" : SkyPackageName.Problem(name);
            if (nameProblem != null)
                Error(manifestName, "\"name\": " + nameProblem);

            var files = new HashSet<string>(PackageFiles(projectRoot), StringComparer.OrdinalIgnoreCase);

            JsonDocument document;
            try
            {
                document = SkyJson.Parse(File.ReadAllBytes(manifestPath));
            }
            catch (JsonException exception)
            {
                Error(manifestName, "not valid JSON: " + exception.Message);
                return problems;
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                Error(manifestName, "cannot be read: " + exception.Message);
                return problems;
            }

            using (document)
            {
                var root = document.RootElement;
                if (root.ValueKind != JsonValueKind.Object)
                {
                    Error(manifestName, "the top level must be an object");
                    return problems;
                }

                if (root.TryGetProperty("format", out var format) && format.ValueKind == JsonValueKind.Number && format.TryGetInt32(out var formatNumber) && formatNumber > 1)
                    Warning(manifestName, $"\"format\": {formatNumber} is newer than this Tweaker knows (1) - the plugin reads it anyway, some of it may be ignored");

                // A referenced file: a name the plugin would agree to read, and one that is in the package.
                void Reference(string layer, string what, string relative)
                {
                    var where = $"layer '{layer}': {what} '{relative}'";
                    var unsafeWhy = UnsafeNameProblem(relative);
                    if (unsafeWhy != null)
                    {
                        Error(manifestName, where + " " + unsafeWhy);
                        return;
                    }
                    var normal = Normalize(relative);
                    if (!files.Contains(normal))
                        Error(manifestName, where + (File.Exists(Path.Combine(projectRoot, normal)) ? " is not packed (a name starting with a dot is left out)" : " is not in the project"));
                }

                if (!root.TryGetProperty("layers", out var layers) || layers.ValueKind != JsonValueKind.Array || layers.GetArrayLength() == 0)
                {
                    Error(manifestName, "no \"layers\" - the sky has nothing to draw");
                    return problems;
                }

                var ids = new HashSet<string>(StringComparer.Ordinal);
                var usable = 0;
                foreach (var layer in layers.EnumerateArray())
                {
                    if (layer.ValueKind != JsonValueKind.Object)
                        continue;

                    var id = SkyJson.Text(layer, "id");
                    if (string.IsNullOrEmpty(id))
                    {
                        Warning(manifestName, "a layer with no \"id\" is ignored by the plugin");
                        continue;
                    }
                    if (!ids.Add(id))
                        Warning(manifestName, $"two layers are called '{id}' - their saved settings will mix");

                    var kind = SkyJson.Text(layer, "kind");
                    var shader = SkyJson.Text(layer, "shader");
                    if (kind == "fullsky")
                    {
                        if (string.IsNullOrEmpty(shader))
                            Error(manifestName, $"layer '{id}': a fullsky layer needs a \"shader\"");
                        else
                            Reference(id, "shader", shader);
                    }
                    else if (kind == "sprites")
                    {
                        // The stem of a pair; none at all draws with the pair built into the plugin.
                        if (!string.IsNullOrEmpty(shader))
                        {
                            Reference(id, "vertex shader", shader + ".vs.hlsl");
                            Reference(id, "pixel shader", shader + ".ps.hlsl");
                        }
                    }
                    else
                    {
                        Error(manifestName, $"layer '{id}': unknown \"kind\" '{kind}' - fullsky or sprites");
                        continue;
                    }
                    usable++;

                    var generator = SkyJson.Text(layer, "generator");
                    if (!string.IsNullOrEmpty(generator))
                        Reference(id, "generator", generator);

                    if (layer.TryGetProperty("fill", out var fill) && fill.ValueKind == JsonValueKind.Object)
                    {
                        var fillKind = SkyJson.Text(fill, "kind") ?? "builtin";
                        switch (fillKind)
                        {
                            case "builtin":
                                break;
                            case "atlas":
                                var fillGenerator = SkyJson.Text(fill, "generator");
                                if (string.IsNullOrEmpty(fillGenerator))
                                    Error(manifestName, $"layer '{id}': fill kind 'atlas' needs a \"generator\" to bake the tiles");
                                else
                                    Reference(id, "fill generator", fillGenerator);
                                break;
                            case "texture":
                                var fillPath = SkyJson.Text(fill, "path");
                                if (string.IsNullOrEmpty(fillPath))
                                    Error(manifestName, $"layer '{id}': fill kind 'texture' needs a \"path\"");
                                else
                                    Reference(id, "fill texture", fillPath);
                                break;
                            case "shader":
                                if (string.IsNullOrEmpty(shader))
                                    Error(manifestName, $"layer '{id}': fill kind 'shader' needs the layer to name its own \"shader\" - the built-in one samples an atlas");
                                break;
                            default:
                                Error(manifestName, $"layer '{id}': unknown fill kind '{fillKind}' - builtin, atlas, texture or shader");
                                break;
                        }
                    }

                    if (layer.TryGetProperty("textures", out var textures) && textures.ValueKind == JsonValueKind.Array)
                    {
                        foreach (var texture in textures.EnumerateArray())
                        {
                            var path = texture.ValueKind == JsonValueKind.Object ? SkyJson.Text(texture, "path") : null;
                            if (string.IsNullOrEmpty(path))
                            {
                                Error(manifestName, $"layer '{id}': a texture with no \"path\"");
                                continue;
                            }
                            if (!path.EndsWith(".dds", StringComparison.OrdinalIgnoreCase))
                                Error(manifestName, $"layer '{id}': texture '{path}' - layer textures are .dds only");
                            Reference(id, "texture", path);
                        }
                    }
                }

                if (usable == 0)
                    Error(manifestName, "no layer the plugin can draw");
            }

            return problems;
        }

        /// <summary>Writes the package as a zip to <paramref name="output"/>. The project is not checked here - <see cref="Check"/> is.</summary>
        public static void Pack(string projectRoot, Stream output, CompressionLevel level)
        {
            using var zip = new ZipArchive(output, ZipArchiveMode.Create, leaveOpen: true);
            foreach (var relative in PackageFiles(projectRoot))
                zip.CreateEntryFromFile(Path.Combine(projectRoot, relative), relative, level);
        }

        /// <summary>
        /// Checks the project and, if nothing is wrong, writes it into the game as &lt;name&gt;.sky
        /// (<paramref name="level"/>: Fastest on every save, the default otherwise). A folder of that name in the way,
        /// or a game without the plugin's folder, is a refusal: neither is ours to move.
        /// </summary>
        public static SkyInstallResult Install(string projectRoot, string engineDirectory, CompressionLevel level = CompressionLevel.Optimal)
        {
            var problems = Check(projectRoot).ToList();
            if (problems.Any(p => p.IsError))
                return new SkyInstallResult { Outcome = SkyInstallOutcome.Problems, Problems = problems };

            if (!SkyPaths.HasPluginFolder(engineDirectory))
            {
                return new SkyInstallResult
                {
                    Outcome = SkyInstallOutcome.Refused, Problems = problems,
                    Message = "The plugin is not installed in this game, so there is nothing to draw the sky. Install it in Settings."
                };
            }

            var fileName = SkyProjects.Read(projectRoot, inSources: false).PackageFileName;
            var folder = SkyPaths.SkyboxesPath(engineDirectory);
            var target = Path.Combine(folder, fileName);
            if (Directory.Exists(target))
            {
                return new SkyInstallResult
                {
                    Outcome = SkyInstallOutcome.Refused, Problems = problems,
                    Message = $"There is a folder called {fileName} in the game's Skyboxes folder, and a file cannot have the same name. "
                              + "Rename this sky, or move that folder out of the way (it may be a sky that came with Tweaker)."
                };
            }

            try
            {
                Directory.CreateDirectory(folder);
                var size = WriteAtomically(projectRoot, target, level);
                AddSizeWarning(projectRoot, size, problems);
                return new SkyInstallResult { Outcome = SkyInstallOutcome.Installed, Problems = problems, Path = target, Size = size };
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                return new SkyInstallResult { Outcome = SkyInstallOutcome.Failed, Problems = problems, Message = "Could not write " + target + ": " + exception.Message };
            }
        }

        /// <summary>The same package written anywhere - "Pack to file", for sharing. Checked the same way.</summary>
        public static SkyInstallResult Export(string projectRoot, string targetPath)
        {
            var problems = Check(projectRoot).ToList();
            if (problems.Any(p => p.IsError))
                return new SkyInstallResult { Outcome = SkyInstallOutcome.Problems, Problems = problems };

            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(targetPath)));
                var size = WriteAtomically(projectRoot, targetPath, CompressionLevel.Optimal);
                AddSizeWarning(projectRoot, size, problems);
                return new SkyInstallResult { Outcome = SkyInstallOutcome.Installed, Problems = problems, Path = targetPath, Size = size };
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                return new SkyInstallResult { Outcome = SkyInstallOutcome.Failed, Problems = problems, Message = "Could not write " + targetPath + ": " + exception.Message };
            }
        }

        /// <summary>
        /// The zip goes to ".&lt;name&gt;.partial" beside the target - a name the plugin's catalog does not list (not .sky,
        /// not an image) - and is then moved over the target in one step. Returns its size.
        /// </summary>
        private static long WriteAtomically(string projectRoot, string target, CompressionLevel level)
        {
            var temporary = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(target)), "." + Path.GetFileName(target) + ".partial");
            try
            {
                using (var stream = new FileStream(temporary, FileMode.Create, FileAccess.Write, FileShare.None))
                    Pack(projectRoot, stream, level);
                var size = new FileInfo(temporary).Length;

                for (var attempt = 1; ; attempt++)
                {
                    try
                    {
                        File.Move(temporary, target, overwrite: true);
                        return size;
                    }
                    catch (IOException) when (attempt < 4)
                    {
                        // The game may be reading the old archive at this very moment.
                        Thread.Sleep(150);
                    }
                }
            }
            finally
            {
                if (File.Exists(temporary))
                {
                    try
                    {
                        File.Delete(temporary);
                    }
                    catch (IOException)
                    {
                    }
                }
            }
        }

        private static void AddSizeWarning(string projectRoot, long size, List<SkyProblem> problems)
        {
            if (size <= SizeWarningBytes)
                return;

            var largest = PackageFiles(projectRoot)
                .Select(relative => (relative, length: new FileInfo(Path.Combine(projectRoot, relative)).Length))
                .OrderByDescending(file => file.length)
                .Take(3)
                .Select(file => $"{file.relative} ({Megabytes(file.length)})");
            problems.Add(new SkyProblem
            {
                Severity = SkyProblemSeverity.Warning,
                File = SkyProjects.ManifestFileName,
                Message = $"the package is {Megabytes(size)} packed, and the game reads it into memory whole - a 32-bit game has little to spare. "
                          + "Largest: " + string.Join(", ", largest)
            });
        }

        private static string Megabytes(long bytes) => (bytes / (1024.0 * 1024.0)).ToString("0.#", System.Globalization.CultureInfo.InvariantCulture) + " MB";

        /// <summary>"shaders\\sky.hlsl", "./shaders/sky.hlsl" -> "shaders/sky.hlsl": how the plugin's package index spells a name.</summary>
        internal static string Normalize(string relative)
        {
            var parts = relative.Replace('\\', '/').Split('/', StringSplitOptions.RemoveEmptyEntries).Where(part => part != ".");
            return string.Join("/", parts);
        }

        /// <summary>
        /// Why the plugin would refuse to read this name from a package (sky_vfs: is_safe_relative_name) - absolute,
        /// "..", a colon, a device name, a trailing dot or space; null when it would read it.
        /// </summary>
        internal static string UnsafeNameProblem(string relative)
        {
            if (string.IsNullOrEmpty(relative))
                return "is empty";
            if (relative[0] == '/' || relative[0] == '\\' || relative.Length >= 2 && relative[1] == ':')
                return "is not a path inside the package";
            foreach (var part in relative.Replace('\\', '/').Split('/', StringSplitOptions.RemoveEmptyEntries))
            {
                if (part == "..")
                    return "leaves the package - a package can only read its own files";
                if (part == ".")
                    continue;
                if (part.IndexOfAny(ForbiddenInComponent) >= 0 || part.Any(char.IsControl))
                    return "has a character a file name cannot have";
                if (Devices.Contains(part.Split('.')[0], StringComparer.OrdinalIgnoreCase))
                    return "names a device, not a file";
                if (part.EndsWith(" ", StringComparison.Ordinal) || part.EndsWith(".", StringComparison.Ordinal))
                    return "ends with a space or a dot, which Windows drops";
            }
            return null;
        }
    }
}
