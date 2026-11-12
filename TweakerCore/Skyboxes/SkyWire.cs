using System;
using System.Globalization;

namespace TweakerCore.Skyboxes
{
    public enum SkyLiveStatus
    {
        Off,
        Waiting,
        Compiling,
        Ready,
        Failed,
        Missing
    }

    public sealed record SkyLiveState(bool Enabled, SkySelection Sky, SkyLiveStatus Status, string Message);

    public sealed record SkyCatalogEntry(SkyKind Kind, string Id, string DisplayName, int FaceSize);

    /// <summary>The SKY_* grammar of protocol 4; no transport or state. See Docs/Internal/overlay-skybox.md.</summary>
    public static class SkyWire
    {
        public const string Prefix = "SKY_";

        public static string Enable(bool enabled) => "SKY_ENABLE " + (enabled ? "true" : "false");

        public static string Select(SkySelection sky) => $"SKY_SELECT {SkyKinds.Key(sky.Kind)} {Uri.EscapeDataString(sky.Id)}";

        public static string Rescan() => "SKY_RESCAN";

        public static bool TryParseEntry(string line, out SkyCatalogEntry entry)
        {
            entry = null;
            var tokens = Split(line);
            if (tokens.Length is < 4 or > 5 || tokens[0] != "SKY_ENTRY" || !SkyKinds.TryParse(tokens[1], out var kind))
                return false;

            var id = Uri.UnescapeDataString(tokens[2]);
            var name = Uri.UnescapeDataString(tokens[3]);
            var size = 0;
            if (id.Length == 0 || id.Contains('\0') || name.Contains('\0')
                || (tokens.Length == 5 && !int.TryParse(tokens[4], NumberStyles.None, CultureInfo.InvariantCulture, out size)))
                return false;

            entry = new SkyCatalogEntry(kind, id, name, size);
            return true;
        }

        public static bool TryParseState(string line, out SkyLiveState state)
        {
            state = null;
            var tokens = Split(line);
            if (tokens.Length is < 5 or > 6 || tokens[0] != "SKY_STATE"
                || (tokens[1] != "true" && tokens[1] != "false") || !SkyKinds.TryParse(tokens[2], out var kind))
                return false;

            var status = tokens[4] switch
            {
                "off" => SkyLiveStatus.Off,
                "waiting" => SkyLiveStatus.Waiting,
                "compiling" => SkyLiveStatus.Compiling,
                "ready" => SkyLiveStatus.Ready,
                "failed" => SkyLiveStatus.Failed,
                "missing" => SkyLiveStatus.Missing,
                _ => (SkyLiveStatus?)null
            };
            var id = Uri.UnescapeDataString(tokens[3]);
            var message = tokens.Length == 6 ? Uri.UnescapeDataString(tokens[5]) : "";
            if (status == null || id.Length == 0 || id.Contains('\0') || message.Contains('\0'))
                return false;

            state = new SkyLiveState(tokens[1] == "true", new SkySelection(kind, id), status.Value, message);
            return true;
        }

        public static bool TryParseSynced(string line, out int count)
        {
            count = 0;
            var tokens = Split(line);
            return tokens.Length == 2 && tokens[0] == "SKY_SYNCED"
                && int.TryParse(tokens[1], NumberStyles.None, CultureInfo.InvariantCulture, out count);
        }

        private static string[] Split(string line) => string.IsNullOrEmpty(line)
            ? Array.Empty<string>() : line.Split(' ', StringSplitOptions.RemoveEmptyEntries);
    }
}
