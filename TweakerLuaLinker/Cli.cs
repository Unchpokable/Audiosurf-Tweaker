using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;

using TweakerLuaLinker.Api;
using TweakerLuaLinker.Diagnostics;
using TweakerLuaLinker.Linking;
using TweakerLuaLinker.Project;

namespace TweakerLuaLinker
{
    /// <summary>
    /// The command-line contract (lua-engine-fix-roadmap.md §9.7). TweakerUI drives this as a child
    /// process, so the parts it relies on are fixed here: the exit codes, and --json carrying exactly the
    /// diagnostics the human output prints.
    /// </summary>
    internal static class Cli
    {
        public const int ExitSuccess = 0;
        public const int ExitBuildFailed = 1;
        public const int ExitUsage = 2;
        public const int ExitInternal = 3;

        private const string Usage =
            "usage: twllink [--project <dir|twproject.json>] [--out <file.lua|dir>] [--install [--game <dir>]]\n" +
            "               [--json] [--api <api.json>]\n" +
            "       twllink --new <template> <dir>\n" +
            "\n" +
            "  Builds a script project into one .lua file for engine\\TweakerStuff\\Scripts\\, plus a\n" +
            "  .lua.map beside it that lets the plugin report errors against your source files.\n" +
            "\n" +
            "  --project   the project folder, or its twproject.json (default: the current folder)\n" +
            "  --out       where to write the script (default: <project>/build/<id>.lua)\n" +
            "  --install   also copy the script and its map into the game's Scripts folder\n" +
            "  --game      the Audiosurf folder for --install (default: the AUDIOSURF_DIR variable)\n" +
            "  --json      print the result as JSON instead of text\n" +
            "  --api       check against this api.json instead of the one twllink was built with\n" +
            "  --new       create a project in <dir> from a template: hud, hook or empty\n" +
            "  --version   print the twllink version\n" +
            "\n" +
            "  exit codes: 0 built, 1 the project has errors, 2 bad command line, 3 twllink itself failed\n";

        private sealed class Options
        {
            public string? Project;
            public string? Out;
            public string? Game;
            public string? ApiPath;
            public string? NewTemplate;
            public string? NewDirectory;
            public bool Json;
            public bool Install;
        }

        public static int Run(string[] args, TextWriter stdout, TextWriter stderr, string currentDirectory)
        {
            var options = new Options();
            for (var i = 0; i < args.Length; i++)
            {
                switch (args[i])
                {
                    case "--project":
                    case "--out":
                    case "--game":
                    case "--api":
                    {
                        if (!TakeValue(args, ref i, out var value))
                            return UsageError(stderr, $"{args[i]} needs a path");
                        switch (args[i - 1])
                        {
                            case "--project": options.Project = value; break;
                            case "--out": options.Out = value; break;
                            case "--game": options.Game = value; break;
                            default: options.ApiPath = value; break;
                        }
                        break;
                    }
                    case "--new":
                        if (!TakeValue(args, ref i, out var template) || !TakeValue(args, ref i, out var directory))
                            return UsageError(stderr, $"--new needs a template and a folder: --new <{string.Join("|", Scaffold.Templates)}> <dir>");
                        options.NewTemplate = template;
                        options.NewDirectory = directory;
                        break;
                    case "--json":
                        options.Json = true;
                        break;
                    case "--install":
                        options.Install = true;
                        break;
                    case "--help":
                    case "-h":
                    case "/?":
                        stdout.Write(Usage);
                        return ExitSuccess;
                    case "--version":
                        stdout.WriteLine($"twllink {Linker.ToolVersion}");
                        return ExitSuccess;
                    default:
                        return UsageError(stderr, $"unknown argument '{args[i]}'");
                }
            }

            if (options.NewTemplate != null)
            {
                if (options.Project != null || options.Out != null || options.Install || options.Json)
                    return UsageError(stderr, "--new creates a project and does nothing else - build it with a second command");
                return New(options, stdout, stderr, currentDirectory);
            }
            if (options.Game != null && !options.Install)
                return UsageError(stderr, "--game only means something with --install");

            var projectRoot = ResolveProject(options.Project, currentDirectory, out var projectError);
            if (projectRoot == null)
                return UsageError(stderr, projectError!);

            ApiDescription? api = null;
            if (options.ApiPath != null)
            {
                var path = Path.GetFullPath(options.ApiPath, currentDirectory);
                try
                {
                    api = ApiDescription.Load(path);
                }
                catch (Exception e) when (e is IOException or UnauthorizedAccessException or JsonException or InvalidDataException or KeyNotFoundException or InvalidOperationException)
                {
                    return UsageError(stderr, $"cannot read {path} as an api.json: {e.Message}");
                }
            }

            return Build(options, projectRoot, api, stdout, currentDirectory);
        }

        private static bool TakeValue(string[] args, ref int i, out string value)
        {
            value = "";
            if (i + 1 >= args.Length || args[i + 1].StartsWith("--", StringComparison.Ordinal))
                return false;
            value = args[++i];
            return true;
        }

        private static int UsageError(TextWriter stderr, string message)
        {
            stderr.WriteLine($"twllink: {message}");
            stderr.WriteLine("run 'twllink --help' for usage");
            return ExitUsage;
        }

        private static int New(Options options, TextWriter stdout, TextWriter stderr, string currentDirectory)
        {
            var directory = Path.GetFullPath(options.NewDirectory!, currentDirectory);
            string? error;
            try
            {
                error = Scaffold.Create(directory, options.NewTemplate!, ApiDescription.Embedded.Api);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                error = $"cannot create the project: {e.Message}";
            }
            if (error != null)
                return UsageError(stderr, error);

            var shown = Path.GetRelativePath(currentDirectory, directory).Replace('\\', '/');
            stdout.WriteLine($"twllink: created a '{options.NewTemplate}' project in {shown}");
            stdout.WriteLine($"  next: cd {shown} && twllink --install");
            return ExitSuccess;
        }

        /// <summary>The project folder; --project may name the folder or the manifest in it, nothing else.</summary>
        private static string? ResolveProject(string? project, string currentDirectory, out string? error)
        {
            error = null;
            var path = Path.GetFullPath(project ?? ".", currentDirectory);

            if (Directory.Exists(path))
            {
                if (File.Exists(Path.Combine(path, Manifest.FileName)))
                    return path;
                error = project == null
                    ? $"no {Manifest.FileName} in the current folder - run twllink in a project folder, pass --project, or make one with --new"
                    : $"no {Manifest.FileName} in {path}";
                return null;
            }
            if (File.Exists(path))
            {
                if (string.Equals(Path.GetFileName(path), Manifest.FileName, StringComparison.OrdinalIgnoreCase))
                    return Path.GetDirectoryName(path)!;
                error = $"{path} is not a project manifest - the manifest is always named {Manifest.FileName}";
                return null;
            }

            error = $"{path} does not exist";
            return null;
        }

        private static string OutputPath(string? option, string projectRoot, string currentDirectory, Manifest manifest)
        {
            if (option == null)
                return Path.Combine(projectRoot, "build", manifest.OutputFileName);

            var path = Path.GetFullPath(option, currentDirectory);
            var isFolder = Directory.Exists(path) || option.EndsWith('/') || option.EndsWith('\\');
            return isFolder ? Path.Combine(path, manifest.OutputFileName) : path;
        }

        // --- one build ------------------------------------------------------------------------------------

        private sealed class Outcome
        {
            public required LinkResult Result;
            public required List<Diagnostic> Diagnostics;
            public string? Output;
            public string? Map;
            public string? Installed;
        }

        private static int Build(Options options, string projectRoot, ApiDescription? api, TextWriter stdout, string currentDirectory)
        {
            var outcome = BuildOnce(options, projectRoot, api, currentDirectory);
            if (options.Json)
                WriteJson(stdout, projectRoot, outcome);
            else
                WriteHuman(stdout, projectRoot, currentDirectory, outcome);
            return outcome.Output != null && (!options.Install || outcome.Installed != null) ? ExitSuccess : ExitBuildFailed;
        }

        private static Outcome BuildOnce(Options options, string projectRoot, ApiDescription? api, string currentDirectory)
        {
            var result = Linker.Link(projectRoot, api);
            var outcome = new Outcome { Result = result, Diagnostics = result.Diagnostics.ToList() };
            if (!result.Success)
                return outcome;

            var output = OutputPath(options.Out, projectRoot, currentDirectory, result.Manifest!);
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(output)!);
                WriteIfChanged(output, result.Bundle!);
                WriteIfChanged(output + ".map", result.Map!);
                outcome.Output = output;
                outcome.Map = output + ".map";
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                outcome.Diagnostics.Add(new Diagnostic(Severity.Error, DiagnosticCodes.OutputWriteFailed, "", 0, 0, $"cannot write {output}: {e.Message}"));
                return outcome;
            }

            // Editor support is not a build input, so a failure here is not a failed build.
            try
            {
                Scaffold.RefreshTooling(projectRoot);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
            }

            if (options.Install)
            {
                var scripts = GameInstall.ScriptsFolder(options.Game, out var error);
                try
                {
                    if (scripts != null)
                        outcome.Installed = GameInstall.Install(scripts, outcome.Output, outcome.Map);
                }
                catch (Exception e) when (e is IOException or UnauthorizedAccessException)
                {
                    error = $"cannot copy into {scripts}: {e.Message}";
                }
                if (outcome.Installed == null)
                    outcome.Diagnostics.Add(new Diagnostic(Severity.Error, DiagnosticCodes.InstallFailed, "", 0, 0, error!));
            }
            return outcome;
        }

        /// <summary>
        /// Leaves an unchanged file alone - including its timestamp, which is what anything watching the project
        /// or the game's Scripts\ folder (TweakerUI) would react to.
        /// </summary>
        private static void WriteIfChanged(string path, string text)
        {
            var bytes = new UTF8Encoding(false).GetBytes(text);
            if (File.Exists(path) && File.ReadAllBytes(path).AsSpan().SequenceEqual(bytes))
                return;
            File.WriteAllBytes(path, bytes);
        }

        // --- output ---------------------------------------------------------------------------------------

        private static string Display(string projectRoot, string currentDirectory, Diagnostic d)
        {
            if (d.File.Length == 0)
                return "twllink";
            return Path.GetRelativePath(currentDirectory, Path.Combine(projectRoot, d.File)).Replace('\\', '/');
        }

        private static void WriteHuman(TextWriter stdout, string projectRoot, string currentDirectory, Outcome outcome)
        {
            var diagnostics = outcome.Diagnostics;

            // file(line,col): severity CODE: message - the shape compilers print and editors already match.
            foreach (var d in diagnostics)
            {
                var severity = d.Severity == Severity.Error ? "error" : "warning";
                var position = d.Line > 0 ? $"({d.Line},{Math.Max(d.Column, 1)})" : "";
                stdout.WriteLine($"{Display(projectRoot, currentDirectory, d)}{position}: {severity} {d.Code}: {d.Message}");
            }

            var errors = diagnostics.Count(d => d.Severity == Severity.Error);
            var warnings = diagnostics.Count - errors;
            var warningText = warnings > 0 ? $", {warnings} {(warnings == 1 ? "warning" : "warnings")}" : "";
            if (outcome.Output != null)
            {
                var result = outcome.Result;
                var manifest = result.Manifest!;
                var lines = result.Bundle!.Count(c => c == '\n');
                var output = Path.GetRelativePath(currentDirectory, outcome.Output).Replace('\\', '/');
                stdout.WriteLine($"twllink: {manifest.Id} {manifest.Version} -> {output} " +
                                 $"({result.Modules.Count} {(result.Modules.Count == 1 ? "module" : "modules")}, {lines} lines{warningText})");
                stdout.WriteLine($"  modules: {string.Join(", ", result.Modules.Select(m => m.Name))}");
                if (outcome.Installed != null)
                    stdout.WriteLine($"  installed: {outcome.Installed}");
            }
            else
            {
                stdout.WriteLine($"twllink: build failed - {errors} {(errors == 1 ? "error" : "errors")}{warningText}");
            }
        }

        private static void WriteJson(TextWriter stdout, string projectRoot, Outcome outcome)
        {
            var result = outcome.Result;
            var diagnostics = outcome.Diagnostics;
            using var stream = new MemoryStream();
            using (var json = new Utf8JsonWriter(stream, new JsonWriterOptions { Indented = true }))
            {
                json.WriteStartObject();
                json.WriteString("twllink", Linker.ToolVersion);
                json.WriteBoolean("success", outcome.Output != null && !diagnostics.Any(d => d.Severity == Severity.Error));
                json.WriteString("project", projectRoot);
                if (result.Manifest != null)
                {
                    json.WriteString("id", result.Manifest.Id);
                    json.WriteString("name", result.Manifest.Name);
                    json.WriteString("version", result.Manifest.Version);
                }
                WriteNullable(json, "output", outcome.Output);
                WriteNullable(json, "map", outcome.Map);
                WriteNullable(json, "installed", outcome.Installed);

                json.WriteStartArray("modules");
                foreach (var module in result.Modules)
                {
                    json.WriteStartObject();
                    json.WriteString("name", module.Name);
                    json.WriteString("file", module.ProjectPath);
                    json.WriteEndObject();
                }
                json.WriteEndArray();

                json.WriteStartArray("diagnostics");
                foreach (var d in diagnostics)
                {
                    json.WriteStartObject();
                    json.WriteString("severity", d.Severity == Severity.Error ? "error" : "warning");
                    json.WriteString("code", d.Code);
                    json.WriteString("file", d.File);
                    json.WriteNumber("line", d.Line);
                    json.WriteNumber("column", d.Column);
                    json.WriteString("message", d.Message);
                    json.WriteEndObject();
                }
                json.WriteEndArray();

                json.WriteNumber("errors", diagnostics.Count(d => d.Severity == Severity.Error));
                json.WriteNumber("warnings", diagnostics.Count(d => d.Severity == Severity.Warning));
                json.WriteEndObject();
            }
            stdout.WriteLine(Encoding.UTF8.GetString(stream.ToArray()));
        }

        private static void WriteNullable(Utf8JsonWriter json, string name, string? value)
        {
            if (value == null)
                json.WriteNull(name);
            else
                json.WriteString(name, value);
        }
    }
}
