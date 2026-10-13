using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace TweakerCore.Scripting
{
    public enum TwllinkSeverity
    {
        Error,
        Warning
    }

    /// <summary>One entry of twllink's <c>diagnostics</c>.</summary>
    public sealed class TwllinkDiagnostic
    {
        public TwllinkSeverity Severity { get; init; }

        /// <summary>"TW0302".</summary>
        public string Code { get; init; }

        /// <summary>Relative to the project root with '/' separators; empty for a message about the project as a whole.</summary>
        public string File { get; init; }

        /// <summary>1-based; 0 when unknown.</summary>
        public int Line { get; init; }

        /// <summary>1-based; 0 when unknown.</summary>
        public int Column { get; init; }

        public string Message { get; init; }
    }

    /// <summary>
    /// What one run of twllink came to. The exit code decides which half is filled
    /// (Docs/Internal/script-manager.md §3): 0 and 1 carry the parsed <c>--json</c> result, 2 and 3 only
    /// <see cref="ToolMessage"/> - the command line was wrong, or twllink itself failed, and neither prints JSON.
    /// </summary>
    public sealed class TwllinkResult
    {
        public const int ExitSuccess = 0;
        public const int ExitBuildFailed = 1;
        public const int ExitUsage = 2;
        public const int ExitInternal = 3;

        /// <summary>The process's exit code, or -1 when it could not be started at all.</summary>
        public int ExitCode { get; init; }

        /// <summary>Built (and installed, when asked).</summary>
        public bool Success => ExitCode == ExitSuccess;

        /// <summary>
        /// Why there is no build result: stderr for exit codes 2 and 3, the reason the process did not start for
        /// -1, a note when stdout was not the JSON it should be. Null when <see cref="Build"/> is set.
        /// </summary>
        public string ToolMessage { get; init; }

        /// <summary>The <c>--json</c> result; null exactly when <see cref="ToolMessage"/> is set.</summary>
        public TwllinkBuild Build { get; init; }
    }

    /// <summary>The <c>--json</c> object of a build, as twllink documents it.</summary>
    public sealed class TwllinkBuild
    {
        public string ToolVersion { get; init; }
        public string Project { get; init; }

        /// <summary>Absent (null) when the manifest could not be read.</summary>
        public string Id { get; init; }
        public string Name { get; init; }
        public string Version { get; init; }

        public string Output { get; init; }
        public string Map { get; init; }

        /// <summary>Where <c>--install</c> put the script; null when not asked or when it failed.</summary>
        public string Installed { get; init; }

        public IReadOnlyList<string> Modules { get; init; }
        public IReadOnlyList<TwllinkDiagnostic> Diagnostics { get; init; }
        public int Errors { get; init; }
        public int Warnings { get; init; }
    }

    /// <summary>
    /// twllink.exe as a child process - the one way TweakerUI builds, installs and creates script projects. It
    /// only speaks the command-line contract (TweakerLuaLinker/Cli.cs); nothing here knows how linking works.
    ///
    /// Every build is run with <c>--json</c>. <c>--new</c> is not: it only ever prints text, and its result is its
    /// exit code plus that text.
    /// </summary>
    public sealed class Twllink
    {
        public const string ExecutableName = "twllink.exe";

        public Twllink(string executablePath)
        {
            ExecutablePath = executablePath;
        }

        public string ExecutablePath { get; }

        public bool Exists => File.Exists(ExecutablePath);

        /// <summary>
        /// Builds the project in <paramref name="projectDirectory"/> into its build\ folder; with
        /// <paramref name="gameDirectory"/> (the Audiosurf folder or its engine\ folder), also installs it there.
        /// </summary>
        public async Task<TwllinkResult> BuildAsync(string projectDirectory, string gameDirectory = null, CancellationToken cancel = default)
        {
            projectDirectory = Path.GetFullPath(projectDirectory);
            var arguments = new List<string> { "--project", projectDirectory, "--json" };
            if (gameDirectory != null)
                arguments.AddRange(new[] { "--install", "--game", gameDirectory });

            var run = await RunAsync(arguments, projectDirectory, cancel).ConfigureAwait(false);
            if (run.ExitCode != TwllinkResult.ExitSuccess && run.ExitCode != TwllinkResult.ExitBuildFailed)
                return new TwllinkResult { ExitCode = run.ExitCode, ToolMessage = Message(run) };

            TwllinkBuild build;
            try
            {
                build = ParseBuild(run.Stdout);
            }
            catch (Exception exception) when (exception is JsonException || exception is InvalidOperationException || exception is KeyNotFoundException)
            {
                return new TwllinkResult
                {
                    ExitCode = run.ExitCode,
                    ToolMessage = "twllink printed something other than its JSON result (" + exception.Message + "):\n" + run.Stdout
                };
            }
            return new TwllinkResult { ExitCode = run.ExitCode, Build = build };
        }

        /// <summary>
        /// Creates a project from a template ("hud", "hook", "empty") in a new or empty folder. The result's
        /// <see cref="TwllinkResult.ToolMessage"/> is what twllink printed, on success as well.
        /// </summary>
        public async Task<TwllinkResult> NewAsync(string template, string directory, CancellationToken cancel = default)
        {
            var full = Path.GetFullPath(directory);
            var run = await RunAsync(new[] { "--new", template, full }, Path.GetDirectoryName(full), cancel).ConfigureAwait(false);
            return new TwllinkResult { ExitCode = run.ExitCode, ToolMessage = Message(run) };
        }

        /// <summary>"1.1", or null when the executable does not answer.</summary>
        public async Task<string> VersionAsync(CancellationToken cancel = default)
        {
            var run = await RunAsync(new[] { "--version" }, null, cancel).ConfigureAwait(false);
            const string prefix = "twllink ";
            var text = run.Stdout.Trim();
            return run.ExitCode == TwllinkResult.ExitSuccess && text.StartsWith(prefix, StringComparison.Ordinal) ? text.Substring(prefix.Length) : null;
        }

        // --- the process ----------------------------------------------------------------------------------

        private sealed class Run
        {
            public int ExitCode;
            public string Stdout = "";
            public string Stderr = "";
        }

        private static string Message(Run run)
        {
            var text = run.Stderr.Trim();
            var stdout = run.Stdout.Trim();
            if (stdout.Length > 0)
                text = text.Length > 0 ? stdout + "\n" + text : stdout;
            return text;
        }

        /// <summary>
        /// The folder itself or its nearest existing parent. A process cannot start in a folder that is not there -
        /// and a new project's parent, or a remembered project that was deleted, is exactly that. twllink gets
        /// full paths, so where it runs only changes how it prints them, and the answer to a missing folder
        /// should be its own "does not exist", not a failure to start.
        /// </summary>
        private static string ExistingFolder(string path)
        {
            for (var folder = path; !string.IsNullOrEmpty(folder); folder = Path.GetDirectoryName(folder))
            {
                if (Directory.Exists(folder))
                    return folder;
            }
            return null;
        }

        private async Task<Run> RunAsync(IEnumerable<string> arguments, string workingDirectory, CancellationToken cancel)
        {
            var info = new ProcessStartInfo(ExecutablePath)
            {
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                StandardOutputEncoding = new UTF8Encoding(false),
                StandardErrorEncoding = new UTF8Encoding(false),
                WorkingDirectory = ExistingFolder(workingDirectory) ?? Path.GetDirectoryName(ExecutablePath) ?? ""
            };
            foreach (var argument in arguments)
                info.ArgumentList.Add(argument);

            using var process = new Process { StartInfo = info };
            try
            {
                if (!process.Start())
                    return new Run { ExitCode = -1, Stderr = "twllink did not start." };
            }
            catch (Exception exception) when (exception is Win32Exception || exception is InvalidOperationException || exception is FileNotFoundException)
            {
                return new Run { ExitCode = -1, Stderr = "Cannot run " + ExecutablePath + ": " + exception.Message };
            }

            var stdout = process.StandardOutput.ReadToEndAsync(cancel);
            var stderr = process.StandardError.ReadToEndAsync(cancel);
            try
            {
                await process.WaitForExitAsync(cancel).ConfigureAwait(false);
            }
            catch (OperationCanceledException)
            {
                try
                {
                    process.Kill();
                }
                catch (InvalidOperationException)
                {
                }
                throw;
            }

            return new Run { ExitCode = process.ExitCode, Stdout = await stdout.ConfigureAwait(false), Stderr = await stderr.ConfigureAwait(false) };
        }

        // --- --json -----------------------------------------------------------------------------------------

        internal static TwllinkBuild ParseBuild(string json)
        {
            using var document = JsonDocument.Parse(json);
            var root = document.RootElement;

            var modules = new List<string>();
            foreach (var module in root.GetProperty("modules").EnumerateArray())
                modules.Add(module.GetProperty("name").GetString());

            var diagnostics = new List<TwllinkDiagnostic>();
            foreach (var d in root.GetProperty("diagnostics").EnumerateArray())
            {
                diagnostics.Add(new TwllinkDiagnostic
                {
                    Severity = d.GetProperty("severity").GetString() == "error" ? TwllinkSeverity.Error : TwllinkSeverity.Warning,
                    Code = d.GetProperty("code").GetString(),
                    File = d.GetProperty("file").GetString(),
                    Line = d.GetProperty("line").GetInt32(),
                    Column = d.GetProperty("column").GetInt32(),
                    Message = d.GetProperty("message").GetString()
                });
            }

            return new TwllinkBuild
            {
                ToolVersion = root.GetProperty("twllink").GetString(),
                Project = root.GetProperty("project").GetString(),
                Id = OptionalString(root, "id"),
                Name = OptionalString(root, "name"),
                Version = OptionalString(root, "version"),
                Output = OptionalString(root, "output"),
                Map = OptionalString(root, "map"),
                Installed = OptionalString(root, "installed"),
                Modules = modules,
                Diagnostics = diagnostics,
                Errors = root.GetProperty("errors").GetInt32(),
                Warnings = root.GetProperty("warnings").GetInt32()
            };
        }

        private static string OptionalString(JsonElement root, string name)
        {
            return root.TryGetProperty(name, out var value) && value.ValueKind == JsonValueKind.String ? value.GetString() : null;
        }
    }
}
