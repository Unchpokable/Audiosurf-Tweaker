using System;
using System.Globalization;

namespace AudiosurfInterface
{
    /// <summary>
    /// Typed view over AudiosurfHandle.MessageResieved's report strings ("asreport songcomplete
    /// 142798", "asreport nowplayingartistname nine inch nails") - strips the "asreport " prefix,
    /// splits on the first space and raises a strongly-typed event per report kind, so callers don't
    /// each hand-parse the same prefixes. The three "nowplaying..." reports always arrive as three
    /// separate consecutive broadcasts for one song start (see GameProtocol_FromDylan.md) - buffered
    /// here and raised as a single NowPlaying event once all three have been seen.
    ///
    /// The prefix strip belongs here and nowhere else: asbridge forwards the game's WM_COPYDATA
    /// payload verbatim (ASBridge/src/service/service.cxx, process_wm_copydata -> BROADCAST_FORWARD),
    /// so what arrives is the raw report line. This class used to assume the prefix had already been
    /// stripped upstream, which silently matched no case at all and swallowed every single report the
    /// game ever sent - the reason QuickPlayer never advanced on songcomplete. AudiosurfHandle's own
    /// registration check uses Contains() rather than an equality test for the same reason.
    /// </summary>
    public sealed class GameReportListener : IDisposable
    {
        public GameReportListener()
        {
            AudiosurfHandle.Instance.MessageResieved += OnMessageResieved;
        }

        public event Action<int> SongCompleted;
        public event Action OnCharacterScreen;
        public event Action<string, string, string> NowPlaying;

        private string _pendingArtist;
        private string _pendingTitle;

        public void Dispose()
        {
            AudiosurfHandle.Instance.MessageResieved -= OnMessageResieved;
        }

        private void OnMessageResieved(object sender, string content)
        {
            ProcessReport(content);
        }

        internal void ProcessReport(string content)
        {
            if (string.IsNullOrEmpty(content))
                return;

            const string reportPrefix = "asreport ";
            if (content.StartsWith(reportPrefix, StringComparison.Ordinal))
                content = content.Substring(reportPrefix.Length);

            var spaceIndex = content.IndexOf(' ');
            var key = spaceIndex < 0 ? content : content.Substring(0, spaceIndex);
            var value = spaceIndex < 0 ? string.Empty : content.Substring(spaceIndex + 1);

            switch (key)
            {
                case GameProtocol.ReportSongComplete:
                    if (int.TryParse(value, NumberStyles.Integer, CultureInfo.InvariantCulture, out var score))
                        SongCompleted?.Invoke(score);
                    break;

                case GameProtocol.ReportOnCharacterScreen:
                    OnCharacterScreen?.Invoke();
                    break;

                case GameProtocol.ReportNowPlayingArtist:
                    _pendingArtist = value;
                    break;

                case GameProtocol.ReportNowPlayingTitle:
                    _pendingTitle = value;
                    break;

                case GameProtocol.ReportNowPlayingAshFile:
                    if (_pendingArtist != null && _pendingTitle != null)
                        NowPlaying?.Invoke(_pendingArtist, _pendingTitle, value);
                    _pendingArtist = null;
                    _pendingTitle = null;
                    break;
            }
        }
    }
}
