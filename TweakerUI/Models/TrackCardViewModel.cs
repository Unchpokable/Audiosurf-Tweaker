using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using AudiosurfInterface;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using QuickPlayerCore;
using QuickPlayerCore.CoverArt;
using QuickPlayerCore.MetadataParsers;
using TweakerUI.Core;
using TweakerUI.ViewModels;

namespace TweakerUI.Models
{
    /// <summary>
    /// Wraps one PlaylistEntry for display/editing in the queue: title/artist/duration/audio
    /// properties (read live from the file via MetadataReader, not persisted - always current, no
    /// staleness if the file changes), the resolved cover, and the right-click Tags/Mods/Character
    /// editors. Any edit rebuilds the entry's Tags/ConfigOverrides/Character from the option lists and
    /// asks the owning QuickPlayerViewModel to persist the playlist - same "rebuild the whole set
    /// rather than track incremental deltas" simplicity used elsewhere in this app.
    /// </summary>
    public partial class TrackCardViewModel : ObservableObject, IDisposable
    {
        private static readonly LocalFileCoverArtProvider _coverProvider = new();
        public TrackCardViewModel(PlaylistEntry entry, QuickPlayerViewModel owner)
        {
            Entry = entry;
            _owner = owner;

            title = entry.SongTitle;
            artist = entry.ArtistName;

            // All three option collections are built before any of them are populated below -
            // TagOptions/ModOptions share the OnTagsOrModsChanged callback, which reads BOTH
            // collections on every single IsEnabled flip (including the ones fired while populating
            // initial state from a saved entry). Building only TagOptions before running its own
            // populate loop left ModOptions null the first time a saved tag's IsEnabled=true fired
            // that callback - instant ArgumentNullException on startup for any entry with a saved
            // tag, never caught because it takes existing saved data to trigger.
            TagOptions = TagOptionViewModel.BuildCatalog(OnTagsOrModsChanged);
            ModOptions = ModOptionViewModel.BuildCatalog(OnTagsOrModsChanged);
            CharacterOptions = CharacterOptionViewModel.BuildRoster(OnCharacterSelected);

            foreach (var tag in entry.Tags)
            {
                var option = TagOptions.FirstOrDefault(o => o.Definition.Token == tag.Token);
                if (option == null)
                    continue;
                // ParameterText before IsEnabled: IsEnabled's setter synchronously fires
                // OnTagsOrModsChanged -> RefreshBadges, which reads ParsedParameter - setting it the
                // other way round meant a restored parameterized tag was briefly "enabled" with no
                // parameter yet, which RefreshBadges' own guard below now tolerates, but there's no
                // reason to pass through that invalid state at all here.
                option.ParameterText = tag.Parameter?.ToString();
                option.IsEnabled = true;
            }

            foreach (var mod in ModOptions)
            {
                if (entry.ConfigOverrides.TryGetValue(mod.Definition.ConfigKey, out var value) && value == mod.Definition.ValueWhenEnabled)
                    mod.IsEnabled = true;
            }

            CharacterOptionViewModel.SelectOnly(CharacterOptions, entry.Character ?? CharacterRoster.RealCharacters[0].Value);
            overrideCharacterEnabled = entry.Character.HasValue;

            RefreshAudioProperties();
            RefreshBadges();
            _ = RefreshCoverAsync();
        }

        public PlaylistEntry Entry { get; }

        [ObservableProperty]
        private string title;

        [ObservableProperty]
        private string artist;

        [ObservableProperty]
        private string durationText = "--:--";

        [ObservableProperty]
        private string audioPropertiesText = string.Empty;

        [ObservableProperty]
        private Bitmap coverBitmap;

        [ObservableProperty]
        private bool isPlaying;

        [ObservableProperty]
        private bool overrideCharacterEnabled;

        // Transient drag-to-reorder state, driven entirely by QuickPlayerView's drag handlers and
        // cleared when the drag ends - nothing here is persisted or read by QuickPlayerCore. It lives
        // on the card rather than in the view because the indicator has to be drawn inside the item
        // template, and Classes.<Name> bindings are how this module already does conditional styling
        // (Classes.Playing).
        [ObservableProperty]
        private bool dropBefore;

        [ObservableProperty]
        private bool dropAfter;

        /// <summary>This card is the one being dragged; the view dims it while it is in flight.</summary>
        [ObservableProperty]
        private bool isDragged;

        public void ClearDropIndicator()
        {
            DropBefore = false;
            DropAfter = false;
        }

        public ObservableCollection<string> ActiveBadges { get; } = new();
        public ObservableCollection<TagOptionViewModel> TagOptions { get; }
        public ObservableCollection<ModOptionViewModel> ModOptions { get; }
        public ObservableCollection<CharacterOptionViewModel> CharacterOptions { get; }

        private readonly QuickPlayerViewModel _owner;

        partial void OnOverrideCharacterEnabledChanged(bool value) => ApplyCharacterOverride();

        [RelayCommand]
        private void Play() => _owner.PlayCard(this);

        [RelayCommand]
        private void Remove() => _owner.RemoveCard(this);

        private void OnCharacterSelected(GameCharacter character)
        {
            CharacterOptionViewModel.SelectOnly(CharacterOptions, character);
            if (OverrideCharacterEnabled)
                ApplyCharacterOverride();
        }

        private void ApplyCharacterOverride()
        {
            Entry.Character = OverrideCharacterEnabled ? CharacterOptions.FirstOrDefault(o => o.IsSelected)?.Value : null;
            RefreshBadges();
            _owner.SaveCurrentPlaylist();
        }

        private void OnTagsOrModsChanged()
        {
            Entry.Tags = TagOptions
                .Where(o => o.IsEnabled && (!o.HasParameter || o.ParsedParameter.HasValue))
                .Select(o => new PlaylistTag { Token = o.Definition.Token, Parameter = o.ParsedParameter })
                .ToList();

            Entry.ConfigOverrides = ModOptions
                .Where(o => o.IsEnabled)
                .ToDictionary(o => o.Definition.ConfigKey, o => o.Definition.ValueWhenEnabled);

            RefreshBadges();
            _owner.SaveCurrentPlaylist();
        }

        private void RefreshBadges()
        {
            ActiveBadges.Clear();

            if (OverrideCharacterEnabled)
            {
                var selected = CharacterOptions.FirstOrDefault(o => o.IsSelected);
                if (selected != null)
                    ActiveBadges.Add(selected.DisplayName);
            }

            foreach (var tag in TagOptions.Where(o => o.IsEnabled))
            {
                // A parameterized tag can be enabled with no parameter yet - e.g. right after the user
                // checks it, before typing into the parameter box (OnIsEnabledChanged fires this
                // synchronously), or transiently while TrackCardViewModel restores a saved entry.
                // Definition.Format() throws for HasParameter tags given no parameter, so there's
                // nothing valid to show yet - skip the badge instead of crashing.
                if (tag.HasParameter && !tag.ParsedParameter.HasValue)
                    continue;

                ActiveBadges.Add(tag.HasParameter
                    ? tag.Definition.Format(tag.ParsedParameter)
                    : tag.BracketPreview ?? tag.Definition.Format());
            }
            foreach (var mod in ModOptions.Where(o => o.IsEnabled))
                ActiveBadges.Add(mod.Label);
        }

        private void RefreshAudioProperties()
        {
            try
            {
                var tags = MetadataReader.ReadData(Entry.FilePath);
                DurationText = FormatDuration(tags.Duration);

                var codec = tags.SamplingParams.Codec.ToString().ToUpperInvariant();
                AudioPropertiesText = tags.IsLossless
                    ? $"{codec} · {(int)tags.SamplingParams.SamplingRate / 1000}kHz · {(int)tags.SamplingParams.BitDepth}bit"
                    : $"{codec} · {tags.SamplingParams.TotalBitrate}kbps";
            }
            catch
            {
                DurationText = "--:--";
                AudioPropertiesText = "Unknown format";
            }
        }

        private static string FormatDuration(TimeSpan duration) => $"{(int)duration.TotalMinutes}:{duration.Seconds:D2}";

        private async Task RefreshCoverAsync()
        {
            var path = Entry.CoverPath;
            if (string.IsNullOrEmpty(path) || !File.Exists(path))
                path = await _coverProvider.ResolveCoverPathAsync(Entry);

            if (string.IsNullOrEmpty(path))
                return;

            Entry.CoverPath = path;

            try
            {
                var bitmap = await Task.Run(() => DecodeCover(path));
                if (bitmap != null)
                    Dispatcher.UIThread.Post(() => AttachCover(bitmap));
            }
            catch (Exception ex)
            {
                // The view falls back to its placeholder icon either way; this is only ever reached
                // by a cover that exists and still cannot be read (truncated file, unsupported
                // encoding), which is worth a line in the log rather than a silent blank tile.
                Logger.Log("TrackCard", $"cover '{path}' could not be decoded: {ex.Message}");
            }
        }

        /// <summary>The queue's cover thumbnail is 52 logical pixels square (Border.QPTrackCover).</summary>
        private const int CoverDisplaySize = 52;

        /// <summary>
        /// Decoded straight to thumbnail size rather than loaded whole and left for the Image to shrink.
        /// Embedded album art is routinely 1000px or larger, and squeezing that into 52px at draw time
        /// samples a handful of source pixels per drawn pixel - which is why the covers came out
        /// speckled and falling apart rather than merely small. Scaling during the decode resamples
        /// properly, and as a side effect a hundred-track playlist stops holding a hundred full-size
        /// bitmaps in memory.
        ///
        /// Three times the display size, not exactly it, so the thumbnails stay sharp on a 150%/200%
        /// display and if the tile is ever made bigger. Scaled by width: cover art that is not square is
        /// rare enough that biasing towards the common case costs nothing, and Stretch=UniformToFill
        /// crops the odd one out anyway.
        /// </summary>
        private static Bitmap DecodeCover(string path)
        {
            using var stream = File.OpenRead(path);
            return Bitmap.DecodeToWidth(stream, CoverDisplaySize * 3, BitmapInterpolationMode.HighQuality);
        }

        /// <summary>
        /// Publishes a freshly decoded cover on the UI thread, releasing whatever it replaces. The
        /// decode is asynchronous, so the card can already be gone by the time it lands (playlist
        /// switched, track removed) - in that case the new bitmap is dropped straight away rather
        /// than resurrecting a disposed card.
        /// </summary>
        private void AttachCover(Bitmap bitmap)
        {
            if (_disposed)
            {
                bitmap.Dispose();
                return;
            }

            var previous = CoverBitmap;
            CoverBitmap = bitmap;
            previous?.Dispose();
        }

        /// <summary>
        /// Releases the decoded cover. Called by <see cref="QuickPlayerViewModel"/> when the card
        /// leaves the queue: the bitmap is Skia-backed unmanaged memory that would otherwise sit
        /// there until a finalizer pass gets around to it.
        /// </summary>
        public void Dispose()
        {
            if (_disposed)
                return;

            _disposed = true;

            // Through the generated property, not the backing field (MVVMTK0034): a view still bound
            // to it has to see the null before the bitmap goes away.
            var bitmap = CoverBitmap;
            CoverBitmap = null;
            bitmap?.Dispose();
        }

        private bool _disposed;
    }
}
