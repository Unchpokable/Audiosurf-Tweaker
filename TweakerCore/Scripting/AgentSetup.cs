using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace TweakerCore.Scripting
{
    /// <summary>Which AI coding agents a project is set up for, and whether the optional binaries skill is in.</summary>
    public sealed record AgentChoice(bool ClaudeCode, bool Codex, bool Cursor, bool Binaries)
    {
        public static AgentChoice Default { get; } = new AgentChoice(true, true, true, false);

        /// <summary>"claude-code,codex,cursor" - how the choice is remembered in settings and in the manifest.</summary>
        public string AgentList => string.Join(",", new[] { ClaudeCode ? "claude-code" : null, Codex ? "codex" : null, Cursor ? "cursor" : null }.Where(a => a != null));

        public static AgentChoice FromList(string agents, bool binaries)
        {
            var set = (agents ?? "").Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries).ToHashSet(StringComparer.OrdinalIgnoreCase);
            return new AgentChoice(set.Contains("claude-code"), set.Contains("codex"), set.Contains("cursor"), binaries);
        }
    }

    public enum AgentSetupState
    {
        /// <summary>No .tweaker\manifest.json: the project was never set up for agents.</summary>
        NotSetUp,
        UpToDate,
        /// <summary>Applying the kit would change something: a newer kit, a file of ours that differs, a changed choice.</summary>
        UpdateAvailable
    }

    /// <summary>What a project's agent setup looks like against a kit - computed with the same decisions Apply makes.</summary>
    public sealed class AgentSetupStatus
    {
        public AgentSetupState State { get; init; }

        /// <summary>The choice the project was set up with; null when it was not.</summary>
        public AgentChoice Choice { get; init; }

        public int KitVersion { get; init; }

        /// <summary>The kit version the project was last set up from; 0 when it was not.</summary>
        public int InstalledKitVersion { get; init; }

        /// <summary>Files of ours the user changed - kept by every update until they ask for the kit's version.</summary>
        public IReadOnlyList<string> Edited { get; init; } = Array.Empty<string>();

        /// <summary>Files an update would add, replace or remove.</summary>
        public IReadOnlyList<string> Pending { get; init; } = Array.Empty<string>();
    }

    public sealed class AgentSetupReport
    {
        public List<string> Added { get; } = new List<string>();
        public List<string> Updated { get; } = new List<string>();
        public List<string> Removed { get; } = new List<string>();

        /// <summary>Changed by the user: left alone. With replaceEdited, these are in <see cref="BackedUp"/> instead.</summary>
        public List<string> KeptEdited { get; } = new List<string>();

        /// <summary>Ours once, deleted by the user since: not brought back.</summary>
        public List<string> SkippedDeleted { get; } = new List<string>();

        /// <summary>Path of the file, and the .old copy the user's version was moved to.</summary>
        public List<(string Path, string Backup)> BackedUp { get; } = new List<(string, string)>();

        public List<string> Errors { get; } = new List<string>();

        public bool Success => Errors.Count == 0;
    }

    /// <summary>
    /// Sets a Script Dev project up for AI coding agents from DevTools\AgentKit, and keeps it up to date
    /// (Docs/Internal/dev-bundle-roadmap.md §3.5, §3.6; what goes where - Tools/AgentKit/README.md).
    ///
    /// Who owns what decides everything here (§2.15). The skills, Claude Code's pointers to them and the copy of the
    /// docs are the Tweaker's: a newer kit replaces them as long as the user has not changed them, a changed one is
    /// kept and reported, a deleted one stays deleted - the same table as PluginInstallation, over a manifest of the
    /// hashes we wrote (.tweaker\manifest.json, committed with the project). AGENTS.md and CLAUDE.md are the user's:
    /// AGENTS.md is written once and afterwards only the block between its markers is ours; CLAUDE.md is written
    /// once, or an existing one gets "@AGENTS.md" in front, and is never touched again. .tweaker\local.json holds the
    /// paths of this computer and is gitignored (§2.17).
    ///
    /// Text is hashed without carriage returns: a project checked out by git with autocrlf has every file with CRLF,
    /// and that must not read as "the user edited all of them".
    /// </summary>
    public static class AgentSetup
    {
        public const string TweakerFolder = ".tweaker";
        public const string ManifestPath = TweakerFolder + "/manifest.json";
        public const string LocalPath = TweakerFolder + "/local.json";
        public const string DocsFolder = TweakerFolder + "/docs";
        public const string SkillsFolder = ".agents/skills";
        public const string ClaudeSkillsFolder = ".claude/skills";
        public const string AgentsFile = "AGENTS.md";
        public const string ClaudeFile = "CLAUDE.md";
        public const string GitignoreFile = ".gitignore";

        public const string BlockBegin = "<!-- tweaker:begin -->";
        public const string BlockEnd = "<!-- tweaker:end -->";

        /// <summary>The manifest's name for the AGENTS.md block - not a file, but owned and hashed like one.</summary>
        private const string BlockKey = "AGENTS.md#tweaker";

        private const string BackupSuffix = ".old";

        private static readonly HashSet<string> TextExtensions = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
        {
            ".md", ".json", ".lua", ".txt", ".yaml", ".yml", ".svg", ".html", ".css"
        };

        public static bool IsSetUp(string projectRoot) => File.Exists(Full(projectRoot, ManifestPath));

        /// <summary>The choice recorded in the project, or null.</summary>
        public static AgentChoice ChoiceOf(string projectRoot)
        {
            var manifest = AgentManifest.Load(projectRoot);
            return manifest.Exists ? manifest.Choice : null;
        }

        public static AgentSetupStatus Status(string projectRoot, AgentKit kit, string docsDirectory)
        {
            var manifest = AgentManifest.Load(projectRoot);
            if (!manifest.Exists)
                return new AgentSetupStatus { State = AgentSetupState.NotSetUp, KitVersion = kit.Version };

            var plan = Plan(projectRoot, kit, docsDirectory, manifest.Choice, manifest, projectName: null);
            var edited = plan.Where(p => p.Decision == Decision.KeepEdited).Select(p => p.Path).ToList();
            var pending = plan.Where(p => p.Decision is Decision.Add or Decision.Overwrite or Decision.Remove or Decision.Forget).Select(p => p.Path).ToList();

            var state = pending.Count > 0 || manifest.Kit != kit.Version ? AgentSetupState.UpdateAvailable : AgentSetupState.UpToDate;
            return new AgentSetupStatus
            {
                State = state,
                Choice = manifest.Choice,
                KitVersion = kit.Version,
                InstalledKitVersion = manifest.Kit,
                Edited = edited,
                Pending = pending
            };
        }

        /// <summary>
        /// Sets the project up, or brings it up to date, for <paramref name="choice"/>. With
        /// <paramref name="replaceEdited"/>, files of ours the user changed get the kit's version too, and theirs is
        /// kept beside it as a .old copy. <paramref name="gameRoot"/> and <paramref name="devtools"/> go into
        /// local.json. File work only - the caller runs it off the UI thread.
        /// </summary>
        public static AgentSetupReport Apply(
            string projectRoot,
            string projectName,
            AgentChoice choice,
            AgentKit kit,
            string docsDirectory,
            string gameRoot,
            string devtools,
            bool replaceEdited = false)
        {
            var report = new AgentSetupReport();
            var manifest = AgentManifest.Load(projectRoot);
            var plan = Plan(projectRoot, kit, docsDirectory, choice, manifest, projectName ?? Path.GetFileName(Path.TrimEndingDirectorySeparator(projectRoot)));

            foreach (var item in plan)
            {
                try
                {
                    Execute(projectRoot, item, manifest, report, replaceEdited);
                }
                catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
                {
                    report.Errors.Add($"{item.Path}: {exception.Message}");
                }
            }

            try
            {
                if (choice.ClaudeCode && !manifest.ClaudeMdHandled)
                {
                    WriteClaudeMd(projectRoot, kit, report);
                    manifest.ClaudeMdHandled = true;
                }

                EnsureGitignore(projectRoot, report);

                manifest.Kit = kit.Version;
                manifest.Choice = choice;
                manifest.Save(projectRoot);

                WriteLocal(projectRoot, gameRoot, devtools);
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                report.Errors.Add(exception.Message);
            }

            return report;
        }

        /// <summary>
        /// .tweaker\local.json: where the game and DevTools are on this computer. Rewritten every time the Tweaker
        /// opens the project, and only for a project that is set up - the others get no .tweaker\ at all.
        /// </summary>
        public static void WriteLocal(string projectRoot, string gameRoot, string devtools)
        {
            if (!IsSetUp(projectRoot))
                return;

            var local = new LocalFile { Format = 1, Game = string.IsNullOrEmpty(gameRoot) ? null : gameRoot, DevTools = devtools };
            var text = JsonSerializer.Serialize(local, new JsonSerializerOptions { WriteIndented = true }).Replace("\r\n", "\n") + "\n";
            var path = Full(projectRoot, LocalPath);
            if (File.Exists(path) && File.ReadAllText(path) == text)
                return; // unchanged: no write, so a watcher on the project sees nothing
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            File.WriteAllText(path, text);
        }

        // --- the plan ---------------------------------------------------------------------------------------

        private enum Decision
        {
            /// <summary>Already exactly the kit's - only the manifest is brought up to date.</summary>
            None,
            Add,
            Overwrite,
            /// <summary>Changed by the user (or theirs to begin with, under our name): theirs stays.</summary>
            KeepEdited,
            /// <summary>Ours once, deleted since.</summary>
            SkipDeleted,
            /// <summary>No longer in the kit (or no longer chosen) and untouched: goes.</summary>
            Remove,
            /// <summary>No longer in the kit but edited, or already gone: stays, and stops being ours.</summary>
            Forget
        }

        private sealed class PlanItem
        {
            public string Path;
            public Decision Decision;
            public byte[] Content;
            public string Hash;

            /// <summary>Set for the AGENTS.md block, which is a region of a file, not a file.</summary>
            public bool IsBlock;
        }

        /// <summary>Everything the kit and the choice want in the project, by project-relative path.</summary>
        private static Dictionary<string, byte[]> Desired(AgentKit kit, string docsDirectory, AgentChoice choice)
        {
            var desired = new Dictionary<string, byte[]>(StringComparer.OrdinalIgnoreCase);

            foreach (var skill in kit.Skills.Where(s => !s.Optional || choice.Binaries))
            {
                foreach (var file in Directory.EnumerateFiles(skill.Folder, "*", SearchOption.AllDirectories))
                    desired[SkillsFolder + "/" + skill.Name + "/" + Relative(skill.Folder, file)] = Read(file);

                if (choice.ClaudeCode)
                {
                    var pointer = kit.Template(AgentKit.ClaudeSkillTemplateFile)
                        .Replace("{{name}}", skill.Name)
                        .Replace("{{description}}", AgentKit.Description(skill) ?? "");
                    desired[ClaudeSkillsFolder + "/" + skill.Name + "/SKILL.md"] = Encoding.UTF8.GetBytes(pointer);
                }
            }

            if (!string.IsNullOrEmpty(docsDirectory) && Directory.Exists(docsDirectory))
            {
                foreach (var file in Directory.EnumerateFiles(docsDirectory, "*", SearchOption.AllDirectories))
                    desired[DocsFolder + "/" + Relative(docsDirectory, file)] = Read(file);
            }

            return desired;
        }

        private static List<PlanItem> Plan(string projectRoot, AgentKit kit, string docsDirectory, AgentChoice choice, AgentManifest manifest, string projectName)
        {
            var plan = new List<PlanItem>();
            var desired = Desired(kit, docsDirectory, choice);

            foreach (var (path, content) in desired.OrderBy(d => d.Key, StringComparer.Ordinal))
            {
                var hash = Hash(path, content);
                plan.Add(new PlanItem { Path = path, Content = content, Hash = hash, Decision = Decide(Full(projectRoot, path), path, hash, manifest) });
            }

            foreach (var path in manifest.Files.Keys.Where(k => k != BlockKey && !desired.ContainsKey(k)).OrderBy(k => k, StringComparer.Ordinal).ToList())
            {
                var target = Full(projectRoot, path);
                var decision = File.Exists(target) && manifest.Files[path] == Hash(path, File.ReadAllBytes(target)) ? Decision.Remove : Decision.Forget;
                plan.Add(new PlanItem { Path = path, Decision = decision });
            }

            plan.Add(PlanBlock(projectRoot, kit, manifest, projectName));
            return plan;
        }

        private static Decision Decide(string target, string path, string hash, AgentManifest manifest)
        {
            var recorded = manifest.Files.TryGetValue(path, out var r) ? r : null;
            if (!File.Exists(target))
                return recorded != null ? Decision.SkipDeleted : Decision.Add;

            var current = Hash(path, File.ReadAllBytes(target));
            if (current == hash)
                return Decision.None;
            return recorded != null && current == recorded ? Decision.Overwrite : Decision.KeepEdited;
        }

        /// <summary>
        /// The AGENTS.md block. A missing AGENTS.md is written from the template - unless we wrote one before, which
        /// means the user deleted it. A file without our markers gets them at its end - unless we put them there before,
        /// which means the user took them out.
        /// </summary>
        private static PlanItem PlanBlock(string projectRoot, AgentKit kit, AgentManifest manifest, string projectName)
        {
            var block = kit.Template(AgentKit.BlockFile).TrimEnd('\n');
            var hash = HashText(block);
            var item = new PlanItem { Path = AgentsFile, Hash = hash, IsBlock = true };
            var recorded = manifest.Files.TryGetValue(BlockKey, out var r) ? r : null;
            var target = Full(projectRoot, AgentsFile);

            if (!File.Exists(target))
            {
                item.Decision = recorded != null ? Decision.SkipDeleted : Decision.Add;
                var whole = kit.Template(AgentKit.AgentsTemplateFile)
                    .Replace("{{project-name}}", projectName ?? "")
                    .Replace("{{block}}", block);
                item.Content = Encoding.UTF8.GetBytes(whole);
                return item;
            }

            var text = File.ReadAllText(target);
            if (!TryFindBlock(text, out _, out _, out var current))
            {
                item.Decision = recorded != null ? Decision.SkipDeleted : Decision.Add;
                item.Content = Encoding.UTF8.GetBytes(block);
                return item;
            }

            var currentHash = HashText(current);
            item.Content = Encoding.UTF8.GetBytes(block);
            item.Decision = currentHash == hash
                ? Decision.None
                : recorded != null && currentHash == recorded ? Decision.Overwrite : Decision.KeepEdited;
            return item;
        }

        // --- doing it -----------------------------------------------------------------------------------------

        private static void Execute(string projectRoot, PlanItem item, AgentManifest manifest, AgentSetupReport report, bool replaceEdited)
        {
            if (item.IsBlock)
            {
                ExecuteBlock(projectRoot, item, manifest, report, replaceEdited);
                return;
            }

            var target = Full(projectRoot, item.Path);
            switch (item.Decision)
            {
                case Decision.None:
                    manifest.Files[item.Path] = item.Hash;
                    return;
                case Decision.Add:
                    Write(target, item.Content);
                    manifest.Files[item.Path] = item.Hash;
                    report.Added.Add(item.Path);
                    return;
                case Decision.Overwrite:
                    Write(target, item.Content);
                    manifest.Files[item.Path] = item.Hash;
                    report.Updated.Add(item.Path);
                    return;
                case Decision.KeepEdited when replaceEdited:
                    var backup = Backup(target);
                    Write(target, item.Content);
                    manifest.Files[item.Path] = item.Hash;
                    report.BackedUp.Add((item.Path, Relative(projectRoot, backup)));
                    return;
                case Decision.KeepEdited:
                    report.KeptEdited.Add(item.Path);
                    return;
                case Decision.SkipDeleted:
                    report.SkippedDeleted.Add(item.Path);
                    return;
                case Decision.Remove:
                    File.Delete(target);
                    RemoveEmptyFolders(projectRoot, Path.GetDirectoryName(target));
                    manifest.Files.Remove(item.Path);
                    report.Removed.Add(item.Path);
                    return;
                case Decision.Forget:
                    manifest.Files.Remove(item.Path);
                    return;
            }
        }

        private static void ExecuteBlock(string projectRoot, PlanItem item, AgentManifest manifest, AgentSetupReport report, bool replaceEdited)
        {
            var target = Full(projectRoot, AgentsFile);
            var block = Encoding.UTF8.GetString(item.Content);

            switch (item.Decision)
            {
                case Decision.None:
                    manifest.Files[BlockKey] = item.Hash;
                    return;
                case Decision.SkipDeleted:
                    report.SkippedDeleted.Add(AgentsFile);
                    return;
                case Decision.KeepEdited when !replaceEdited:
                    report.KeptEdited.Add(AgentsFile);
                    return;
            }

            if (!File.Exists(target))
            {
                Write(target, item.Content);
                manifest.Files[BlockKey] = item.Hash;
                report.Added.Add(AgentsFile);
                return;
            }

            var text = File.ReadAllText(target);
            var newline = text.Contains("\r\n") ? "\r\n" : "\n";
            var region = BlockBegin + "\n" + block + "\n" + BlockEnd;

            string updated;
            if (TryFindBlock(text, out var start, out var end, out _))
            {
                if (item.Decision == Decision.KeepEdited)
                    report.BackedUp.Add((AgentsFile, Relative(projectRoot, Backup(target, keepOriginal: true))));
                else
                    report.Updated.Add(AgentsFile);
                updated = text[..start] + region.Replace("\n", newline) + text[end..];
            }
            else
            {
                // The user's AGENTS.md, ours appended: their text first, a blank line, then the block.
                updated = text.TrimEnd('\r', '\n') + newline + newline + region.Replace("\n", newline) + newline;
                report.Added.Add(AgentsFile);
            }

            File.WriteAllText(target, updated, new UTF8Encoding(false));
            manifest.Files[BlockKey] = item.Hash;
        }

        /// <summary>The markers and what is between them, without the line breaks right inside them.</summary>
        private static bool TryFindBlock(string text, out int start, out int end, out string content)
        {
            start = text.IndexOf(BlockBegin, StringComparison.Ordinal);
            var close = start < 0 ? -1 : text.IndexOf(BlockEnd, start + BlockBegin.Length, StringComparison.Ordinal);
            if (start < 0 || close < 0)
            {
                end = -1;
                content = null;
                return false;
            }

            end = close + BlockEnd.Length;
            content = AgentKit.Normalize(text[(start + BlockBegin.Length)..close]).Trim('\n');
            return true;
        }

        /// <summary>CLAUDE.md, once: the template if there is none, "@AGENTS.md" in front of one that lacks it.</summary>
        private static void WriteClaudeMd(string projectRoot, AgentKit kit, AgentSetupReport report)
        {
            var target = Full(projectRoot, ClaudeFile);
            if (!File.Exists(target))
            {
                Write(target, Encoding.UTF8.GetBytes(kit.Template(AgentKit.ClaudeTemplateFile)));
                report.Added.Add(ClaudeFile);
                return;
            }

            var text = File.ReadAllText(target);
            if (text.Split('\n').Any(line => line.Trim() == "@AGENTS.md"))
                return;
            var newline = text.Contains("\r\n") ? "\r\n" : "\n";
            File.WriteAllText(target, "@AGENTS.md" + newline + newline + text, new UTF8Encoding(false));
            report.Updated.Add(ClaudeFile);
        }

        /// <summary>local.json must never be committed: it holds this computer's paths (§2.17, §2.20).</summary>
        private static void EnsureGitignore(string projectRoot, AgentSetupReport report)
        {
            var target = Full(projectRoot, GitignoreFile);
            var text = File.Exists(target) ? File.ReadAllText(target) : "";
            var lines = text.Split('\n').Select(l => l.Trim());
            if (lines.Any(l => l == LocalPath || l == "/" + LocalPath || l == TweakerFolder + "/" + "local.json"))
                return;

            var newline = text.Contains("\r\n") ? "\r\n" : "\n";
            var prefix = text.Length == 0 || text.EndsWith("\n", StringComparison.Ordinal) ? "" : newline;
            File.WriteAllText(target,
                text + prefix + "# Audiosurf Tweaker: paths of this computer, rewritten whenever the Tweaker opens the project" + newline + LocalPath + newline,
                new UTF8Encoding(false));
            (text.Length == 0 ? report.Added : report.Updated).Add(GitignoreFile);
        }

        private static string Backup(string target, bool keepOriginal = false)
        {
            var candidate = target + BackupSuffix;
            for (var attempt = 2; File.Exists(candidate) || Directory.Exists(candidate); ++attempt)
                candidate = target + BackupSuffix + "." + attempt.ToString(CultureInfo.InvariantCulture);
            if (keepOriginal)
                File.Copy(target, candidate);
            else
                File.Move(target, candidate);
            return candidate;
        }

        private static void Write(string target, byte[] content)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(target));
            File.WriteAllBytes(target, content);
        }

        /// <summary>Folders a removed skill leaves behind, up to (not including) the project itself.</summary>
        private static void RemoveEmptyFolders(string projectRoot, string folder)
        {
            var root = Path.TrimEndingDirectorySeparator(Path.GetFullPath(projectRoot));
            while (!string.IsNullOrEmpty(folder)
                   && Path.GetFullPath(folder).Length > root.Length
                   && Directory.Exists(folder)
                   && !Directory.EnumerateFileSystemEntries(folder).Any())
            {
                Directory.Delete(folder);
                folder = Path.GetDirectoryName(folder);
            }
        }

        // --- hashing and paths -------------------------------------------------------------------------------

        private static string Hash(string path, byte[] content) =>
            TextExtensions.Contains(Path.GetExtension(path)) ? HashText(Encoding.UTF8.GetString(content)) : HashBytes(content);

        private static string HashText(string text) => HashBytes(Encoding.UTF8.GetBytes(text.Replace("\r", "")));

        private static string HashBytes(byte[] bytes) => Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();

        /// <summary>Kit files are LF; written as they are, so a fresh project has exactly what the kit has.</summary>
        private static byte[] Read(string file) => File.ReadAllBytes(file);

        private static string Full(string projectRoot, string relative) =>
            Path.Combine(projectRoot, relative.Replace('/', Path.DirectorySeparatorChar));

        private static string Relative(string root, string file) => Path.GetRelativePath(root, file).Replace('\\', '/');

        // --- the files of .tweaker\ -----------------------------------------------------------------------------

        private sealed class LocalFile
        {
            [JsonPropertyName("format")]
            public int Format { get; set; }

            [JsonPropertyName("game")]
            public string Game { get; set; }

            [JsonPropertyName("devtools")]
            public string DevTools { get; set; }
        }

        /// <summary>.tweaker\manifest.json - what of the project is ours, with the hash of what we wrote.</summary>
        private sealed class AgentManifest
        {
            [JsonPropertyName("format")]
            public int Format { get; set; } = 1;

            [JsonPropertyName("kit")]
            public int Kit { get; set; }

            [JsonPropertyName("agents")]
            public string Agents { get; set; } = "";

            [JsonPropertyName("binaries")]
            public bool Binaries { get; set; }

            /// <summary>CLAUDE.md was written or given its import once; it is the user's from then on.</summary>
            [JsonPropertyName("claude_md")]
            public bool ClaudeMdHandled { get; set; }

            /// <summary>Project-relative path ('/' separators) to the SHA-256 of what we wrote there.</summary>
            [JsonPropertyName("files")]
            public Dictionary<string, string> Files { get; set; } = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

            [JsonIgnore]
            public bool Exists { get; private set; }

            [JsonIgnore]
            public AgentChoice Choice
            {
                get => AgentChoice.FromList(Agents, Binaries);
                set
                {
                    Agents = value.AgentList;
                    Binaries = value.Binaries;
                }
            }

            /// <summary>A missing or unreadable manifest is an empty one: everything on disk then counts as the user's.</summary>
            public static AgentManifest Load(string projectRoot)
            {
                var path = Full(projectRoot, ManifestPath);
                if (!File.Exists(path))
                    return new AgentManifest();
                try
                {
                    var loaded = JsonSerializer.Deserialize<AgentManifest>(File.ReadAllText(path)) ?? new AgentManifest();
                    loaded.Files = new Dictionary<string, string>(loaded.Files ?? new Dictionary<string, string>(), StringComparer.OrdinalIgnoreCase);
                    loaded.Exists = true;
                    return loaded;
                }
                catch (Exception exception) when (exception is JsonException || exception is IOException)
                {
                    return new AgentManifest { Exists = true };
                }
            }

            public void Save(string projectRoot)
            {
                var path = Full(projectRoot, ManifestPath);
                Directory.CreateDirectory(Path.GetDirectoryName(path));
                var sorted = new AgentManifest
                {
                    Format = Format, Kit = Kit, Agents = Agents, Binaries = Binaries, ClaudeMdHandled = ClaudeMdHandled,
                    Files = Files.OrderBy(f => f.Key, StringComparer.Ordinal).ToDictionary(f => f.Key, f => f.Value)
                };
                // Sorted and LF: the manifest is committed, and a diff of it should show what changed, not reorderings.
                File.WriteAllText(path, JsonSerializer.Serialize(sorted, new JsonSerializerOptions { WriteIndented = true }).Replace("\r\n", "\n") + "\n");
                Exists = true;
            }
        }
    }
}
