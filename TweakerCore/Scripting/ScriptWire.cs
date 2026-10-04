using System;

namespace TweakerCore.Scripting
{
    /// <summary>What the running plugin says a script is doing (Docs/Internal/overlay-scripts.md, "Статус").</summary>
    public enum ScriptLiveStatus
    {
        Off,
        Running,

        /// <summary>Failed to load. With Enabled true it is on and broken; with false an enable was asked for and did not take.</summary>
        Failed,

        /// <summary>Stopped by the error or time budget; Message says which.</summary>
        Suspended,

        /// <summary>The plugin has no such file - gone at its last rescan, or never seen.</summary>
        Missing
    }

    /// <summary>One SCRIPT_STATE line: a script's state as the plugin reports it.</summary>
    public sealed record ScriptLiveState(string FileName, bool Enabled, ScriptLiveStatus Status, string Message);

    /// <summary>
    /// The SCRIPT_* half of the TW_OVL grammar, host side (Docs/Internal/overlay-scripts.md). Text in, text out:
    /// no transport and no state, so both directions are checked by TweakerCore.Tests against the exact lines
    /// the plugin's harness (TweakerPlugin/harness/remote) expects.
    ///
    /// File names and messages are percent-encoded UTF-8 - Uri.EscapeDataString is the same unreserved set the
    /// plugin's ui/wire_text leaves alone.
    /// </summary>
    public static class ScriptWire
    {
        public const string Prefix = "SCRIPT_";

        private const string SetOp = "SCRIPT_SET";
        private const string ReloadOp = "SCRIPT_RELOAD";
        private const string RescanOp = "SCRIPT_RESCAN";
        private const string StateOp = "SCRIPT_STATE";
        private const string SyncedOp = "SCRIPT_SYNCED";

        public static string Set(string fileName, bool enabled) =>
            $"{SetOp} {Uri.EscapeDataString(fileName)} {(enabled ? "true" : "false")}";

        public static string Reload(string fileName) => $"{ReloadOp} {Uri.EscapeDataString(fileName)}";

        public static string Rescan() => RescanOp;

        /// <summary>"SCRIPT_STATE &lt;file&gt; &lt;true|false&gt; &lt;status&gt; [message]". Anything else, or a status this build does not know, is not a state.</summary>
        public static bool TryParseState(string line, out ScriptLiveState state)
        {
            state = null;
            var tokens = Split(line);
            if (tokens.Length < 4 || tokens[0] != StateOp || tokens[1].Length == 0)
                return false;

            bool enabled;
            if (tokens[2] == "true")
                enabled = true;
            else if (tokens[2] == "false")
                enabled = false;
            else
                return false;

            ScriptLiveStatus status;
            switch (tokens[3])
            {
                case "off": status = ScriptLiveStatus.Off; break;
                case "running": status = ScriptLiveStatus.Running; break;
                case "failed": status = ScriptLiveStatus.Failed; break;
                case "suspended": status = ScriptLiveStatus.Suspended; break;
                case "missing": status = ScriptLiveStatus.Missing; break;
                default: return false;
            }

            var message = tokens.Length > 4 ? Unescape(tokens[4]) : "";
            state = new ScriptLiveState(Unescape(tokens[1]), enabled, status, message);
            return true;
        }

        /// <summary>"SCRIPT_SYNCED &lt;count&gt;" - the end of a full list.</summary>
        public static bool TryParseSynced(string line, out int count)
        {
            count = 0;
            var tokens = Split(line);
            return tokens.Length >= 2 && tokens[0] == SyncedOp
                   && int.TryParse(tokens[1], System.Globalization.NumberStyles.None, System.Globalization.CultureInfo.InvariantCulture, out count);
        }

        private static string[] Split(string line) =>
            string.IsNullOrEmpty(line) ? Array.Empty<string>() : line.Split(' ', StringSplitOptions.RemoveEmptyEntries);

        /// <summary>Tolerant like the plugin's percent_decode: Uri leaves a malformed escape in place as text.</summary>
        private static string Unescape(string token) => Uri.UnescapeDataString(token);
    }
}
