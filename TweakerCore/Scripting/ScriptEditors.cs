using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;

namespace TweakerCore.Scripting
{
    /// <summary>
    /// An editor the Script Dev tab can open a project in (Docs/Internal/script-manager-roadmap.md §5, п. 2):
    /// where it is installed and how to ask it for a folder, or a file at a line.
    /// </summary>
    public sealed class ScriptEditor
    {
        internal ScriptEditor(EditorDefinition definition, string executablePath)
        {
            Definition = definition;
            ExecutablePath = executablePath;
        }

        internal EditorDefinition Definition { get; }

        /// <summary>"vscode", "cursor", "sublime", "notepadpp", "neovim", "vim" - what the choice is remembered by.</summary>
        public string Id => Definition.Id;

        public string Name => Definition.Name;

        public string ExecutablePath { get; }

        /// <summary>Runs in a console window of its own (vim, Neovim) rather than a window of its own making.</summary>
        public bool IsConsole => Definition.IsConsole;

        /// <summary>Can be asked for a file at a line and column; one that cannot gets the file, or the folder.</summary>
        public bool OpensAtLine => Definition.OpensAtLine;

        /// <summary>
        /// Keeps one process for all its windows, so a second launch hands its arguments to the first and exits -
        /// and the first, started long ago by somebody else, has an environment without twllink in PATH. Only
        /// editors with a terminal inside care: without one, PATH reaches nothing the user types into.
        /// </summary>
        public bool TerminalMissesPathWhenRunning => Definition.ProcessName != null && Definition.HasTerminal;

        /// <summary>Whether an instance of this editor is running right now.</summary>
        public bool IsRunning()
        {
            if (Definition.ProcessName == null)
                return false;
            var processes = Process.GetProcessesByName(Definition.ProcessName);
            foreach (var process in processes)
                process.Dispose();
            return processes.Length > 0;
        }
    }

    /// <summary>Where to look for editors - the system's folders by default, fake ones in tests.</summary>
    public sealed class EditorSearch
    {
        public string LocalAppData { get; init; }
        public string ProgramFiles { get; init; }
        public string ProgramFilesX86 { get; init; }
        public string UserProfile { get; init; }

        /// <summary>The PATH variable: editors installed elsewhere are found through their command-line shims.</summary>
        public string PathVariable { get; init; }

        public static EditorSearch FromSystem()
        {
            return new EditorSearch
            {
                LocalAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                ProgramFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),
                ProgramFilesX86 = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
                UserProfile = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
                PathVariable = Environment.GetEnvironmentVariable("PATH") ?? ""
            };
        }
    }

    internal sealed class EditorDefinition
    {
        public string Id;
        public string Name;
        public bool IsConsole;
        public bool OpensAtLine;
        public bool HasTerminal;

        /// <summary>For "is it already running"; null for an editor that runs one process per window.</summary>
        public string ProcessName;

        /// <summary>Install locations, each relative to one of the search roots.</summary>
        public Func<EditorSearch, IEnumerable<string>> Candidates;

        /// <summary>A command-line shim on PATH, and how to get from it to the editor itself.</summary>
        public string PathShim;
        public string FromShim;

        public Func<string, IReadOnlyList<string>> ProjectArguments;
        public Func<string, string, int, int, IReadOnlyList<string>> FileArguments;
    }

    /// <summary>
    /// The editors the Script Dev tab knows (§5, п. 2, decided 28.09.2026): VS Code, Cursor, Sublime Text,
    /// Notepad++, Vim and Neovim. Found in their default install locations and through the shims they put on PATH;
    /// adding one is one more definition below.
    ///
    /// Launching is where twllink gets into the user's hands: the editor starts with DevTools\Binaries at the front
    /// of PATH and AUDIOSURF_DIR pointing at the game, so a terminal inside it runs "twllink --install" as it is.
    /// The user's own PATH is never touched (the owner's decision: writing a rarely used tool into it is not ours
    /// to do, and an editor that is already running would not see it anyway).
    /// </summary>
    public static class ScriptEditors
    {
        private static IReadOnlyList<string> Args(params string[] arguments) => arguments;

        internal static readonly EditorDefinition[] Definitions =
        {
            // VS Code and Cursor (a fork of it) take the same command line. "-g file:line:col" goes to the line; with
            // the folder first, the file opens in that folder's window rather than in whichever window was last.
            new EditorDefinition
            {
                Id = "vscode", Name = "Visual Studio Code", OpensAtLine = true, HasTerminal = true, ProcessName = "Code",
                Candidates = s => new[]
                {
                    Combine(s.LocalAppData, "Programs", "Microsoft VS Code", "Code.exe"),
                    Combine(s.ProgramFiles, "Microsoft VS Code", "Code.exe")
                },
                PathShim = "code.cmd", FromShim = Path.Combine("..", "Code.exe"),
                ProjectArguments = project => Args(project),
                FileArguments = (project, file, line, column) => Args(project, "-g", file + ":" + line + ":" + column)
            },
            new EditorDefinition
            {
                Id = "cursor", Name = "Cursor", OpensAtLine = true, HasTerminal = true, ProcessName = "Cursor",
                Candidates = s => new[]
                {
                    Combine(s.LocalAppData, "Programs", "cursor", "Cursor.exe"),
                    Combine(s.ProgramFiles, "cursor", "Cursor.exe")
                },
                PathShim = "cursor.cmd", FromShim = Path.Combine("..", "..", "..", "Cursor.exe"),
                ProjectArguments = project => Args(project),
                FileArguments = (project, file, line, column) => Args(project, "-g", file + ":" + line + ":" + column)
            },
            // No terminal of its own (without packages), so there is no PATH to worry about when it is running.
            new EditorDefinition
            {
                Id = "sublime", Name = "Sublime Text", OpensAtLine = true, ProcessName = "sublime_text",
                Candidates = s => new[]
                {
                    Combine(s.ProgramFiles, "Sublime Text", "sublime_text.exe"),
                    Combine(s.ProgramFiles, "Sublime Text 3", "sublime_text.exe")
                },
                PathShim = "subl.exe", FromShim = "sublime_text.exe",
                ProjectArguments = project => Args(project),
                FileArguments = (project, file, line, column) => Args(project, file + ":" + line + ":" + column)
            },
            new EditorDefinition
            {
                Id = "notepadpp", Name = "Notepad++", OpensAtLine = true,
                Candidates = s => new[]
                {
                    Combine(s.ProgramFiles, "Notepad++", "notepad++.exe"),
                    Combine(s.ProgramFilesX86, "Notepad++", "notepad++.exe")
                },
                PathShim = "notepad++.exe", FromShim = "notepad++.exe",
                ProjectArguments = project => Args("-openFoldersAsWorkspace", project),
                FileArguments = (project, file, line, column) => Args("-n" + line, "-c" + column, file)
            },
            // Console editors run in the project folder, in a console window of their own: their environment is the
            // one this launch gives them, every time - :terminal and :! inside see twllink.
            new EditorDefinition
            {
                Id = "neovim", Name = "Neovim", IsConsole = true, OpensAtLine = true,
                Candidates = s => new[]
                {
                    Combine(s.ProgramFiles, "Neovim", "bin", "nvim.exe"),
                    Combine(s.LocalAppData, "Programs", "Neovim", "bin", "nvim.exe"),
                    Combine(s.UserProfile, "scoop", "apps", "neovim", "current", "bin", "nvim.exe")
                },
                PathShim = "nvim.exe", FromShim = "nvim.exe",
                ProjectArguments = project => Args("."),
                FileArguments = (project, file, line, column) => Args("+call cursor(" + line + "," + column + ")", file)
            },
            new EditorDefinition
            {
                Id = "vim", Name = "Vim", IsConsole = true, OpensAtLine = true,
                Candidates = s => VimFolders(s.ProgramFiles).Concat(VimFolders(s.ProgramFilesX86)),
                PathShim = "vim.exe", FromShim = "vim.exe",
                ProjectArguments = project => Args("."),
                FileArguments = (project, file, line, column) => Args("+call cursor(" + line + "," + column + ")", file)
            }
        };

        /// <summary>The installed editors, in the order above; each found once however many ways lead to it.</summary>
        public static IReadOnlyList<ScriptEditor> Detect(EditorSearch search)
        {
            var result = new List<ScriptEditor>();
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var pathFolders = (search.PathVariable ?? "").Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);

            foreach (var definition in Definitions)
            {
                var found = definition.Candidates(search).FirstOrDefault(File.Exists);
                if (found == null)
                {
                    found = pathFolders
                        .Where(folder => !IsGitsOwnTools(folder))
                        .Select(folder => Path.Combine(folder.Trim('"'), definition.PathShim))
                        .Where(File.Exists)
                        .Select(shim => Path.GetFullPath(Path.Combine(Path.GetDirectoryName(shim)!, definition.FromShim)))
                        .FirstOrDefault(File.Exists);
                }

                if (found != null && seen.Add(Path.GetFullPath(found)))
                    result.Add(new ScriptEditor(definition, Path.GetFullPath(found)));
            }
            return result;
        }

        /// <summary>
        /// How to start <paramref name="editor"/> on a project, or on one of its files at a line
        /// (<paramref name="file"/> relative to the project, '/' separators, as twllink reports it; null for the
        /// project alone). <paramref name="line"/>/<paramref name="column"/> of 0 mean "unknown" and open at the top.
        /// </summary>
        public static ProcessStartInfo StartInfo(
            ScriptEditor editor,
            string project,
            string file,
            int line,
            int column,
            string twllinkFolder,
            string gameFolder)
        {
            var info = new ProcessStartInfo(editor.ExecutablePath)
            {
                UseShellExecute = false,
                // A console editor started by a windowed app gets a console of its own this way (or a Windows
                // Terminal tab, where that is the default terminal) - with the environment set below.
                CreateNoWindow = false,
                WorkingDirectory = project
            };

            IReadOnlyList<string> arguments;
            if (file == null)
            {
                arguments = editor.Definition.ProjectArguments(project);
            }
            else
            {
                var full = Path.GetFullPath(Path.Combine(project, file.Replace('/', Path.DirectorySeparatorChar)));
                arguments = editor.OpensAtLine
                    ? editor.Definition.FileArguments(project, full, Math.Max(line, 1), Math.Max(column, 1))
                    : editor.Definition.ProjectArguments(project);
            }
            foreach (var argument in arguments)
                info.ArgumentList.Add(argument);

            var path = info.Environment.TryGetValue("PATH", out var existing) ? existing : "";
            info.Environment["PATH"] = string.IsNullOrEmpty(path) ? twllinkFolder : twllinkFolder + Path.PathSeparator + path;
            if (gameFolder != null)
                info.Environment["AUDIOSURF_DIR"] = gameFolder;
            return info;
        }

        /// <summary>Starts the editor; null on success, the reason otherwise. The process handle is the caller's.</summary>
        public static string Launch(ProcessStartInfo info, out Process process)
        {
            process = null;
            try
            {
                process = Process.Start(info);
                return process == null ? "the editor did not start" : null;
            }
            catch (Exception exception) when (exception is Win32Exception || exception is InvalidOperationException || exception is FileNotFoundException)
            {
                return exception.Message;
            }
        }

        /// <summary>
        /// Git for Windows' usr\bin: its vim is the MSYS build Git uses for commit messages, running on the MSYS
        /// runtime with its own idea of paths and environment - not an editor the user installed, and not one to
        /// hand a Windows project to. It is on PATH whenever Git Bash is, which is how it got into the list once.
        /// </summary>
        private static bool IsGitsOwnTools(string folder)
        {
            var trimmed = Path.TrimEndingDirectorySeparator(folder.Trim('"'));
            return trimmed.EndsWith(Path.Combine("usr", "bin"), StringComparison.OrdinalIgnoreCase)
                   && File.Exists(Path.Combine(trimmed, "..", "..", "git-bash.exe"));
        }

        private static string Combine(string root, params string[] parts)
        {
            return string.IsNullOrEmpty(root) ? null : Path.Combine(new[] { root }.Concat(parts).ToArray());
        }

        /// <summary>The Vim installer puts it in "Vim\vim91\" - the version is in the folder name. Newest first.</summary>
        private static IEnumerable<string> VimFolders(string programFiles)
        {
            var vim = string.IsNullOrEmpty(programFiles) ? null : Path.Combine(programFiles, "Vim");
            if (vim == null || !Directory.Exists(vim))
                return Array.Empty<string>();
            return Directory.EnumerateDirectories(vim, "vim*")
                .OrderByDescending(folder => folder, StringComparer.OrdinalIgnoreCase)
                .Select(folder => Path.Combine(folder, "vim.exe"));
        }
    }
}
