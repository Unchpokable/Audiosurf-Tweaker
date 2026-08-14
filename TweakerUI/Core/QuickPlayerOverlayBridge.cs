using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.ComponentModel;
using System.Linq;
using System.Text;
using Avalonia.Threading;
using AudiosurfInterface;
using QuickPlayerCore;
using QuickPlayerCore.Audiosurf;
using TweakerUI.Models;
using TweakerUI.ViewModels;

namespace TweakerUI.Core
{
    /// <summary>
    /// The host half of the Quick Player overlay protocol (Docs/Internal/overlay-quickplayer.md):
    /// serializes Quick Player state into QP_* ops and applies the QP_NOTIFY_* ops coming back.
    ///
    /// Separate from OverlayHelper on purpose. That class is the TW_OVL transport - inject guard,
    /// handshake, ready state - and Quick Player adds a dozen ops with their own grammar; folding
    /// them in would do to it exactly what putting the player in menu.cxx would have done to the
    /// plugin's menu. OverlayHelper therefore exposes only SendQuickPlayer/QuickPlayerRequested and
    /// never looks inside a payload.
    ///
    /// Owned by QuickPlayerViewModel, which is also the only thing that can actually carry out an
    /// overlay request: playlist edits have to go through the view models (a TrackCardViewModel
    /// rebuilds its badges and persists), not straight into the QuickPlayerCore model.
    /// </summary>
    internal sealed class QuickPlayerOverlayBridge : IDisposable
    {
        /// <summary>
        /// How many upcoming entries QP_ORDER carries. The op is re-sent on every track start, so an
        /// unbounded queue would put the whole remaining pass - hundreds of ids - on the wire each
        /// time. Nothing renders this list yet; it exists so a Next/Prev request can be confirmed
        /// against the exact entry the host is about to start, and as groundwork for a custom song
        /// announcer. See Docs/Internal/overlay-quickplayer.md.
        /// </summary>
        private const int UpcomingLimit = 25;

        /// <summary>
        /// Stands in for a field with no value: no per-track character, no queue position, a tag with
        /// no parameter. L3 splits on spaces, so an empty token would not survive the trip - it would
        /// vanish and shift every token after it out of place.
        /// </summary>
        private const string NoneToken = "-";

        internal QuickPlayerOverlayBridge(QuickPlayerViewModel owner, PlaybackController playback)
        {
            _owner = owner;
            _playback = playback;

            OverlayHelper.OverlayReady += OnOverlayReady;
            OverlayHelper.QuickPlayerRequested += OnQuickPlayerRequested;

            _owner.Playlists.CollectionChanged += OnPlaylistsChanged;
            _owner.PlaylistChanged += OnPlaylistChanged;
            _owner.PropertyChanged += OnOwnerPropertyChanged;

            _playback.PlaybackOrderChanged += OnPlaybackOrderChanged;
            _playback.EntryStarted += OnEntryStarted;
            _playback.EntryEnded += OnEntryEnded;

            foreach (var row in _owner.Playlists)
                Watch(row);
        }

        private readonly QuickPlayerViewModel _owner;
        private readonly PlaybackController _playback;

        // Which playlist the overlay has open. Tracks are fetched lazily, one playlist at a time
        // (QP_NOTIFY_SELECT), because a user with a hundred playlists of a thousand tracks would
        // otherwise spend seconds pushing them all at handshake. Null means the overlay has not
        // asked for any yet, and nothing track-shaped is pushed at all.
        private Guid? _overlayPlaylistId;

        private bool _disposed;

        /// <summary>
        /// Everything the overlay needs on connect except track lists, which stay lazy. Called on the
        /// handshake and never again - later changes arrive through the individual events below.
        /// </summary>
        private void PushSnapshot()
        {
            PushCatalog();
            PushDefaultCharacter();
            PushOrder();
            PushPlaybackState();
        }

        private void PushCatalog()
        {
            var payload = new StringBuilder("QP_CATALOG ");
            payload.Append(_owner.Playlists.Count);

            foreach (var row in _owner.Playlists)
            {
                payload.Append(' ').Append(Id(row.Playlist.Id));
                payload.Append(' ').Append(Text(row.Name));
                payload.Append(' ').Append(row.Playlist.Entries.Count);
            }

            OverlayHelper.SendQuickPlayer(payload.ToString());
        }

        private void PushTracks(Playlist playlist)
        {
            if (playlist == null)
                return;

            // Entries are collected before the count is written: a null entry (possible out of
            // hand-edited playlist JSON, which is why Playlist.LoadFromPath null-guards too) is
            // skipped, and a count that disagreed with the records after it would desync the
            // plugin's whole parse, not just that one track.
            var records = playlist.Entries
                .Where(entry => entry != null)
                .Select(FormatTrack)
                .ToList();

            var payload = new StringBuilder("QP_TRACKS ");
            payload.Append(Id(playlist.Id));
            payload.Append(' ').Append(records.Count);
            foreach (var record in records)
                payload.Append(' ').Append(record);

            OverlayHelper.SendQuickPlayer(payload.ToString());
        }

        /// <summary>
        /// &lt;entryId&gt; &lt;artist&gt; &lt;title&gt; &lt;character|-&gt; &lt;tagCount&gt;
        /// [&lt;tag&gt; &lt;param|-&gt;] &lt;modCount&gt; [&lt;mod&gt;]
        /// </summary>
        private static string FormatTrack(PlaylistEntry entry)
        {
            var record = new StringBuilder();
            record.Append(Id(entry.Id));
            record.Append(' ').Append(Text(entry.ArtistName));
            record.Append(' ').Append(Text(entry.SongTitle));
            record.Append(' ').Append(entry.Character.HasValue ? entry.Character.Value.ToString() : NoneToken);

            var tags = entry.Tags ?? new List<PlaylistTag>();
            record.Append(' ').Append(tags.Count);
            foreach (var tag in tags)
            {
                record.Append(' ').Append(tag.Token.ToString());
                record.Append(' ').Append(tag.Parameter.HasValue ? tag.Parameter.Value.ToString() : NoneToken);
            }

            // A track's "mods" are the same seven tweaks the overlay already knows, which is why the
            // plugin needs no catalog of its own for them. Only the keys actually set to their
            // enabled value are sent, matching how ModOptionViewModel decides a mod is on: a stored
            // key holding the *default* value is not an override the user asked for.
            var mods = (entry.ConfigOverrides ?? new Dictionary<string, bool>())
                .Select(pair => new { Definition = GameTweakCatalog.FindByConfigKey(pair.Key), pair.Value })
                .Where(mod => mod.Definition != null && mod.Value == mod.Definition.ConfigValueWhenEnabled)
                .Select(mod => mod.Definition.WireName)
                .ToList();

            record.Append(' ').Append(mods.Count);
            foreach (var mod in mods)
                record.Append(' ').Append(mod);

            return record.ToString();
        }

        /// <summary>
        /// Describes the playlist the overlay has open, not the one being played - its mode and
        /// advance trigger are what the overlay's chips display. The position and upcoming queue are
        /// only filled in when those are the same playlist; otherwise the module has no position in
        /// it and says so, the same distinction PlaybackController.CurrentIndexIn makes.
        /// </summary>
        private void PushOrder()
        {
            var playlist = OverlayPlaylist() ?? _owner.SelectedPlaylistRow?.Playlist;
            if (playlist == null)
                return;

            var driving = _playback.CurrentPlaylistId == playlist.Id;
            var current = driving ? _playback.CurrentEntry : null;
            var upcoming = driving ? _playback.GetUpcoming(UpcomingLimit) : Array.Empty<PlaylistEntry>();

            var payload = new StringBuilder("QP_ORDER ");
            payload.Append(Id(playlist.Id));
            payload.Append(' ').Append(playlist.Mode.ToString());
            payload.Append(' ').Append(playlist.AdvanceOn == AdvanceTrigger.CharacterScreen ? "manual" : "auto");
            payload.Append(' ').Append(current != null ? Id(current.Id) : NoneToken);
            payload.Append(' ').Append(upcoming.Count);
            foreach (var entry in upcoming)
                payload.Append(' ').Append(Id(entry.Id));

            OverlayHelper.SendQuickPlayer(payload.ToString());
        }

        private void PushPlaybackState()
        {
            var playlistId = _playback.CurrentPlaylistId;
            var entry = _playback.CurrentEntry;

            if (!_playback.IsActive || playlistId == null || entry == null)
            {
                OverlayHelper.SendQuickPlayer("QP_STOPPED");
                return;
            }

            OverlayHelper.SendQuickPlayer($"QP_NOWPLAYING {Id(playlistId.Value)} {Id(entry.Id)}");
        }

        private void PushDefaultCharacter() =>
            OverlayHelper.SendQuickPlayer($"QP_DEFAULT_CHARACTER {_owner.DefaultCharacter}");

        private void OnOverlayReady(object sender, EventArgs e) => OnUiThread(PushSnapshot);

        private void OnPlaylistsChanged(object sender, NotifyCollectionChangedEventArgs e)
        {
            foreach (var row in e.OldItems?.OfType<PlaylistRowViewModel>() ?? Enumerable.Empty<PlaylistRowViewModel>())
                row.PropertyChanged -= OnPlaylistRowPropertyChanged;

            foreach (var row in e.NewItems?.OfType<PlaylistRowViewModel>() ?? Enumerable.Empty<PlaylistRowViewModel>())
                Watch(row);

            PushCatalog();
        }

        private void Watch(PlaylistRowViewModel row) => row.PropertyChanged += OnPlaylistRowPropertyChanged;

        // A rename writes straight through PlaylistRowViewModel.ApplyRename (its own Playlist.Save,
        // not the view model's SaveCurrentPlaylist), so it never reaches PlaylistChanged below - the
        // catalog would keep showing the old name until something unrelated happened.
        private void OnPlaylistRowPropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(PlaylistRowViewModel.Name) || e.PropertyName == nameof(PlaylistRowViewModel.TrackCount))
                PushCatalog();
        }

        private void OnOwnerPropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(QuickPlayerViewModel.DefaultCharacter))
                PushDefaultCharacter();
        }

        // Fired by SaveCurrentPlaylist, which every edit already funnels through: tags, mods, the
        // per-track character, adding and removing tracks, reordering, mode and advance trigger. This
        // is also the confirming echo a QP_NOTIFY_* reverse-sync request waits for.
        private void OnPlaylistChanged(object sender, Playlist playlist)
        {
            PushCatalog();
            PushOrder();

            if (playlist != null && _overlayPlaylistId == playlist.Id)
                PushTracks(playlist);
        }

        private void OnPlaybackOrderChanged() => OnUiThread(PushOrder);

        private void OnEntryStarted(PlaylistEntry entry) => OnUiThread(PushPlaybackState);

        // Deliberately reports the gap between tracks rather than suppressing it. An auto-advance
        // really does leave nothing playing while the next file is prepared (TempFileTagger can copy
        // and retag, which takes real time), and claiming otherwise would need a timer here to guess
        // whether another start is coming.
        private void OnEntryEnded(PlaylistEntry entry) => OnUiThread(PushPlaybackState);

        private void OnQuickPlayerRequested(object sender, string payload)
        {
            var space = payload.IndexOf(' ');
            var op = space < 0 ? payload : payload.Substring(0, space);
            var rest = space < 0 ? string.Empty : payload.Substring(space + 1).Trim();
            var tokens = rest.Length == 0 ? Array.Empty<string>() : rest.Split(' ');

            // Everything below runs on the UI thread and goes through the same view models the
            // desktop uses. Touching the QuickPlayerCore model directly would be shorter and wrong:
            // a TrackCardViewModel is what rebuilds a card's badges and persists the playlist, so an
            // edit that skipped it would leave the desktop showing stale state until the user
            // happened to reselect the playlist.
            OnUiThread(() => Apply(op, tokens));
        }

        private void Apply(string op, string[] tokens)
        {
            switch (op)
            {
                case "QP_NOTIFY_SELECT" when tokens.Length >= 1:
                    SelectPlaylist(tokens[0]);
                    break;

                case "QP_NOTIFY_PLAY" when tokens.Length >= 2:
                    SelectPlaylist(tokens[0]);
                    var card = FindCard(tokens[1]);
                    if (card != null)
                        _ = _owner.PlayCard(card);
                    break;

                case "QP_NOTIFY_TRANSPORT" when tokens.Length >= 1:
                    ApplyTransport(tokens[0]);
                    break;

                case "QP_NOTIFY_MODE" when tokens.Length >= 2:
                    SelectPlaylist(tokens[0]);
                    if (Enum.TryParse<PlaybackMode>(tokens[1], out var mode))
                        _owner.PlaybackModeOptions.FirstOrDefault(o => o.Value == mode)?.SelectCommand.Execute(null);
                    break;

                case "QP_NOTIFY_ADVANCE" when tokens.Length >= 2:
                    SelectPlaylist(tokens[0]);
                    // Written through the same pair of properties the desktop radio buttons bind to,
                    // each of which acts only on being set true (see QuickPlayerViewModel).
                    if (tokens[1] == "manual")
                        _owner.IsAdvanceManual = true;
                    else
                        _owner.IsAdvanceAuto = true;
                    break;

                case "QP_NOTIFY_DEFAULT_CHARACTER" when tokens.Length >= 1:
                    if (Enum.TryParse<GameCharacter>(tokens[0], out var defaultCharacter))
                        _owner.DefaultCharacter = defaultCharacter;
                    break;

                case "QP_NOTIFY_TAG" when tokens.Length >= 3:
                    ApplyTag(tokens);
                    break;

                case "QP_NOTIFY_MOD" when tokens.Length >= 3:
                    ApplyMod(tokens);
                    break;

                case "QP_NOTIFY_CHARACTER" when tokens.Length >= 2:
                    ApplyCharacter(tokens);
                    break;
            }
        }

        private void ApplyTransport(string action)
        {
            switch (action)
            {
                case "next":
                    _owner.NextCommand.Execute(null);
                    break;
                case "prev":
                    _owner.PrevCommand.Execute(null);
                    break;
                case "stop":
                    _owner.StopCommand.Execute(null);
                    break;
            }
        }

        // "<entryId> <token> <true|false> [param|-]"
        private void ApplyTag(string[] tokens)
        {
            var card = FindCard(tokens[0]);
            if (card == null || !Enum.TryParse<SongTagToken>(tokens[1], out var token))
                return;

            var option = card.TagOptions.FirstOrDefault(o => o.Definition.Token == token);
            if (option == null)
                return;

            var enabled = tokens[2] == "true";

            // ParameterText before IsEnabled, for the same reason TrackCardViewModel's own
            // constructor does it in that order: IsEnabled's setter synchronously rebuilds the
            // entry's tags, and a parameterized tag switched on with no parameter yet would be
            // dropped by that rebuild.
            if (tokens.Length >= 4 && tokens[3] != NoneToken)
                option.ParameterText = tokens[3];

            option.IsEnabled = enabled;
        }

        // "<entryId> <wireName> <true|false>"
        private void ApplyMod(string[] tokens)
        {
            var card = FindCard(tokens[0]);
            var definition = GameTweakCatalog.FindByWireName(tokens[1]);
            if (card == null || definition == null)
                return;

            var option = card.ModOptions.FirstOrDefault(o => o.Definition.ConfigKey == definition.ConfigKey);
            if (option != null)
                option.IsEnabled = tokens[2] == "true";
        }

        // "<entryId> <character|->"
        private void ApplyCharacter(string[] tokens)
        {
            var card = FindCard(tokens[0]);
            if (card == null)
                return;

            if (tokens[1] == NoneToken)
            {
                card.OverrideCharacterEnabled = false;
                return;
            }

            if (!Enum.TryParse<GameCharacter>(tokens[1], out var character))
                return;

            // The option's own command, not SelectOnly: it is what notifies the card, and the card
            // is what rebuilds the entry and saves.
            card.CharacterOptions.FirstOrDefault(o => o.Value == character)?.SelectCommand.Execute(null);
            card.OverrideCharacterEnabled = true;
        }

        // Cards only exist for the selected playlist, which is exactly why an overlay selection
        // moves the desktop's (see SelectPlaylist).
        private TrackCardViewModel FindCard(string entryId) =>
            _owner.Queue.FirstOrDefault(card => Id(card.Entry.Id) == entryId);

        /// <summary>
        /// The overlay opened a playlist. This moves the desktop's selection too, and that is the
        /// intended behaviour rather than a side effect: an edit from the overlay has to go through
        /// the TrackCardViewModel that owns the entry, and those only exist for the selected playlist
        /// (Queue is rebuilt on selection change). Two independently selected playlists would leave
        /// the desktop's badges stale until the user happened to switch back.
        /// </summary>
        private void SelectPlaylist(string playlistId)
        {
            var row = _owner.Playlists.FirstOrDefault(candidate => Id(candidate.Playlist.Id) == playlistId);
            if (row == null)
                return;

            _overlayPlaylistId = row.Playlist.Id;

            if (_owner.SelectedPlaylistRow != row)
                _owner.SelectedPlaylistRow = row;

            PushTracks(row.Playlist);
            PushOrder();
        }

        private Playlist OverlayPlaylist() =>
            _overlayPlaylistId.HasValue
                ? _owner.Playlists.FirstOrDefault(row => row.Playlist.Id == _overlayPlaylistId.Value)?.Playlist
                : null;

        // PlaybackController raises its events from whatever thread the game report arrived on, and
        // every push below walks ObservableCollections the UI owns. Posting rather than checking
        // CheckAccess first keeps one ordering for both cases - a push queued from a report can never
        // overtake one queued from the UI.
        private static void OnUiThread(Action action) => Dispatcher.UIThread.Post(action);

        private static string Id(Guid id) => id.ToString("N");

        /// <summary>
        /// Percent-encoded free text. An empty result is replaced with an encoded space, because an
        /// empty token cannot exist on the wire (see <see cref="NoneToken"/>) - a track with no
        /// artist tag would otherwise shift its own title into the character field and corrupt the
        /// rest of the message. NoneToken itself is not usable here: "-" is an unreserved character,
        /// so a genuine title of "-" would encode to the same thing.
        /// </summary>
        private static string Text(string value)
        {
            var encoded = Uri.EscapeDataString(value ?? string.Empty);
            return encoded.Length > 0 ? encoded : "%20";
        }

        public void Dispose()
        {
            if (_disposed)
                return;
            _disposed = true;

            OverlayHelper.OverlayReady -= OnOverlayReady;
            OverlayHelper.QuickPlayerRequested -= OnQuickPlayerRequested;

            _owner.Playlists.CollectionChanged -= OnPlaylistsChanged;
            _owner.PlaylistChanged -= OnPlaylistChanged;
            _owner.PropertyChanged -= OnOwnerPropertyChanged;

            foreach (var row in _owner.Playlists)
                row.PropertyChanged -= OnPlaylistRowPropertyChanged;

            _playback.PlaybackOrderChanged -= OnPlaybackOrderChanged;
            _playback.EntryStarted -= OnEntryStarted;
            _playback.EntryEnded -= OnEntryEnded;
        }
    }
}
