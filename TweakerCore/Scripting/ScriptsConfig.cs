using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace TweakerCore.Scripting
{
    /// <summary>
    /// engine\TweakerStuff\Config\scripts.cfg - which scripts the plugin should not run. The plugin owns this
    /// file (TweakerPlugin/src/lua/lua_config.cxx): it reads it once at start and rewrites it on every toggle
    /// in the overlay. The host writes it only while the game is not running - then there is no plugin to
    /// ask, and nothing that could read the file halfway through (Docs/Internal/script-manager.md
    /// §8.4). Checking that is the caller's job, as with PluginInstallation.
    ///
    /// Reading follows the plugin exactly: <c>script.&lt;file&gt;=0|false|off</c> disables the script, anything
    /// else means nothing, and names compare case-sensitively - the plugin keys a script by the file name as
    /// the directory listing spells it.
    ///
    /// Writing is an edit, not a rewrite: only the <c>script.&lt;file&gt;</c> lines of the script being switched
    /// change. The plugin's own rewrite keeps the disabled list and the <c>engine.*</c> tuning lines; the host
    /// has no reason to be the one that drops a comment or a line it does not understand.
    /// </summary>
    public sealed class ScriptsConfig
    {
        public const string FileName = "scripts.cfg";
        public const string ConfigFolderName = "Config";

        private const string ScriptKeyPrefix = "script.";

        private static readonly string[] Header =
        {
            "# Scripts the overlay should not run. Anything not listed here is enabled, so a new",
            "# .lua dropped into Scripts\\ starts working without editing this file."
        };

        private readonly string _path;
        private readonly List<string> _lines;

        private ScriptsConfig(string path, List<string> lines)
        {
            _path = path;
            _lines = lines;
        }

        public static string PathFor(string engineDirectory)
        {
            return Path.Combine(engineDirectory, PluginInstall.PluginInstallation.StuffFolderName, ConfigFolderName, FileName);
        }

        /// <summary>A missing file is the normal first run: every script enabled.</summary>
        public static ScriptsConfig Load(string engineDirectory)
        {
            var path = PathFor(engineDirectory);
            var lines = File.Exists(path) ? File.ReadAllLines(path, new UTF8Encoding(false)).ToList() : new List<string>();
            return new ScriptsConfig(path, lines);
        }

        public bool IsEnabled(string scriptFileName)
        {
            return !_lines.Any(line => Parse(line, out var name, out var value) && name == scriptFileName && Disables(value));
        }

        /// <summary>The file names this config switches off, in file order.</summary>
        public IReadOnlyList<string> Disabled
        {
            get
            {
                var result = new List<string>();
                foreach (var line in _lines)
                {
                    if (Parse(line, out var name, out var value) && Disables(value) && !result.Contains(name))
                        result.Add(name);
                }
                return result;
            }
        }

        /// <summary>Changes the config in memory; <see cref="Save"/> writes it.</summary>
        public void SetEnabled(string scriptFileName, bool enabled)
        {
            if (enabled)
            {
                _lines.RemoveAll(line => Parse(line, out var name, out _) && name == scriptFileName);
                return;
            }

            if (!IsEnabled(scriptFileName))
                return;
            if (_lines.Count == 0)
                _lines.AddRange(Header);

            // After the last script line, so the list stays together above the engine.* block the plugin writes.
            var at = _lines.FindLastIndex(line => Parse(line, out _, out _));
            _lines.Insert(at < 0 ? LastHeaderComment() + 1 : at + 1, ScriptKeyPrefix + scriptFileName + "=0");
        }

        public void Save()
        {
            Directory.CreateDirectory(Path.GetDirectoryName(_path));
            var text = new StringBuilder();
            foreach (var line in _lines)
                text.Append(line).Append('\n');
            File.WriteAllText(_path, text.ToString(), new UTF8Encoding(false));
        }

        /// <summary>Where a first script line goes when there is none: after the leading comment block.</summary>
        private int LastHeaderComment()
        {
            var last = -1;
            for (var i = 0; i < _lines.Count && _lines[i].TrimStart().StartsWith('#'); i++)
                last = i;
            return last;
        }

        /// <summary>The plugin's reading of one line: trimmed, <c>#</c> comments, <c>key=value</c>, both halves trimmed.</summary>
        private static bool Parse(string line, out string name, out string value)
        {
            name = null;
            value = null;

            var text = line.AsSpan().Trim(" \t\r");
            if (text.IsEmpty || text[0] == '#')
                return false;

            var eq = text.IndexOf('=');
            if (eq < 0)
                return false;

            var key = text.Slice(0, eq).Trim(" \t\r");
            if (!key.StartsWith(ScriptKeyPrefix, StringComparison.Ordinal) || key.Length == ScriptKeyPrefix.Length)
                return false;

            name = key.Slice(ScriptKeyPrefix.Length).ToString();
            value = text.Slice(eq + 1).Trim(" \t\r").ToString();
            return true;
        }

        private static bool Disables(string value)
        {
            return value == "0" || value == "false" || value == "off";
        }
    }
}
