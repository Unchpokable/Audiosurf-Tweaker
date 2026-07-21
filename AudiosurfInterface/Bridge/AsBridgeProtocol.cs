using System.Collections.Generic;
using System.Text;

namespace AudiosurfInterface.Bridge
{
    public enum AsBridgeReportType
    {
        Ok,
        Failed,
        OverlayOk,
        OverlayFailed,
        BroadcastForward,
        OverlayForward,
        Service
    }

    public sealed class AsBridgeReport
    {
        public AsBridgeReport(AsBridgeReportType type, List<string> details)
        {
            Type = type;
            Details = details;
        }

        public AsBridgeReportType Type { get; }
        public List<string> Details { get; }
    }

    /// <summary>
    /// Managed mirror of the asbridge text protocol (ASBridge/src/proto/message.hxx):
    /// client sends "CCOMMAND SEND \"command\"", server replies with
    /// "SREPORT OK|FAILED|BROADCAST_FORWARD|SERVICE \"detail\" ..." - details are double-quoted,
    /// whitespace-separated, no escaping.
    /// </summary>
    public static class AsBridgeProtocol
    {
        public const string ServiceStatusWindowFound = "WINDOW_FOUND";
        public const string ServiceStatusWindowLost = "WINDOW_LOST";

        public static string SerializeSend(string command)
        {
            return $"CCOMMAND SEND \"{command}\" ";
        }

        public static string SerializeOverlaySend(string payload)
        {
            return $"CCOMMAND OVERLAY_SEND \"{payload}\" ";
        }

        public static bool TryParseReport(string raw, out AsBridgeReport report)
        {
            report = null;
            if (string.IsNullOrWhiteSpace(raw))
                return false;

            var rest = raw.TrimStart(' ');

            if (!TakeWord(ref rest, out var header) || header != "SREPORT")
                return false;

            if (!TakeWord(ref rest, out var msgToken))
                return false;

            AsBridgeReportType type;
            switch (msgToken)
            {
                case "OK": type = AsBridgeReportType.Ok; break;
                case "FAILED": type = AsBridgeReportType.Failed; break;
                case "OVERLAY_OK": type = AsBridgeReportType.OverlayOk; break;
                case "OVERLAY_FAILED": type = AsBridgeReportType.OverlayFailed; break;
                case "BROADCAST_FORWARD": type = AsBridgeReportType.BroadcastForward; break;
                case "OVERLAY_FORWARD": type = AsBridgeReportType.OverlayForward; break;
                case "SERVICE": type = AsBridgeReportType.Service; break;
                default: return false;
            }

            if (!TryParseDetails(rest, out var details))
                return false;

            report = new AsBridgeReport(type, details);
            return true;
        }

        private static bool TakeWord(ref string s, out string word)
        {
            word = null;
            var start = 0;
            while (start < s.Length && s[start] == ' ')
                start++;
            if (start >= s.Length)
                return false;

            var end = s.IndexOf(' ', start);
            if (end < 0)
            {
                word = s.Substring(start);
                s = string.Empty;
            }
            else
            {
                word = s.Substring(start, end - start);
                s = s.Substring(end + 1);
            }

            return true;
        }

        private static bool TryParseDetails(string s, out List<string> details)
        {
            details = new List<string>();
            var pos = 0;
            while (true)
            {
                while (pos < s.Length && s[pos] == ' ')
                    pos++;
                if (pos >= s.Length)
                    return true;
                if (s[pos] != '"')
                    return false;

                var close = s.IndexOf('"', pos + 1);
                if (close < 0)
                    return false;

                details.Add(s.Substring(pos + 1, close - pos - 1));
                pos = close + 1;
            }
        }
    }
}
