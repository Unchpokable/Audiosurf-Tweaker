using System;
using System.Collections.Generic;
using System.Linq;
using AudiosurfInterface;

namespace QuickPlayerCore.Audiosurf
{
    /// <summary>
    /// Every song tag keyword from GameProtocol_SongTags.md. Most only exist as a bracketed suffix
    /// the game reads out of the file's title tag (see TempFileTagger) - SidewinderCamera and
    /// BankingCamera are the two exceptions that also have a live asconfig equivalent
    /// (GameProtocol.Sidewinder/UseBankingCamera), so a playlist entry using those resolves to a
    /// GameConfigState override instead of a title rewrite (see PlaybackController).
    /// </summary>
    public enum SongTagToken
    {
        FourLanes,
        Portal,
        MonoOnly,
        EverybodyMono,
        MonoLessGrey,
        MonoAllGrey,
        MonoNoGrey,
        MonoBasePoints,
        NoStealth,
        SidewinderCamera,
        BankingCamera,
        FirstPerson,
        Caterpillar,
        MinimumMatchSize,
        WhitesBlacksPercent,
        MatchCollectionTicks,
        PuzzleRowsCount,
        HidePuzzleGrid,
        Steep
    }

    public sealed class SongTagDefinition
    {
        internal SongTagDefinition(SongTagToken token, string description, Func<int?, string> format,
            bool hasParameter = false, int minParameter = 0, int maxParameter = int.MaxValue,
            (string ConfigKey, bool Value)? asConfigBinding = null)
        {
            Token = token;
            Description = description;
            HasParameter = hasParameter;
            MinParameter = minParameter;
            MaxParameter = maxParameter;
            AsConfigBinding = asConfigBinding;
            _format = format;
        }

        public SongTagToken Token { get; }
        public string Description { get; }
        public bool HasParameter { get; }
        public int MinParameter { get; }
        public int MaxParameter { get; }

        /// <summary>Non-null only for tags that resolve to a live asconfig override instead of a title rewrite.</summary>
        public (string ConfigKey, bool Value)? AsConfigBinding { get; }

        private readonly Func<int?, string> _format;

        public string Format(int? parameter = null)
        {
            if (HasParameter)
            {
                if (parameter is null)
                    throw new ArgumentException($"Tag '{Token}' requires a parameter", nameof(parameter));
                if (parameter < MinParameter || parameter > MaxParameter)
                    throw new ArgumentOutOfRangeException(nameof(parameter), parameter,
                        $"Tag '{Token}' accepts values {MinParameter}-{MaxParameter}");
            }

            return _format(parameter);
        }
    }

    public static class SongTagCatalog
    {
        public static IReadOnlyList<SongTagDefinition> All { get; } = new List<SongTagDefinition>
        {
            new(SongTagToken.FourLanes, "Play with 4 lanes", _ => "[as-4lane]"),
            new(SongTagToken.Portal, "Portal Mode", _ => "[as-portal]"),
            new(SongTagToken.MonoOnly, "Mono characters required", _ => "[as-monoonly]"),
            new(SongTagToken.EverybodyMono, "All characters have only 2 block colors", _ => "[as-everybodymono]"),
            new(SongTagToken.MonoLessGrey, "Mono characters have less grey blocks", _ => "[as-lessgrey]"),
            new(SongTagToken.MonoAllGrey, "Mono all greys - scores start at 1000 and go down with each hit", _ => "[as-allgrey]"),
            new(SongTagToken.MonoNoGrey, "Mono characters have no grey blocks", _ => "[as-nogrey]"),
            new(SongTagToken.MonoBasePoints, "Base points for Mono characters (default 3)", p => $"[as-monopt{p}]", hasParameter: true, minParameter: 0),
            new(SongTagToken.NoStealth, "Remove Stealth and cumulative hit bonuses", _ => "[as-nostlth]"),
            new(SongTagToken.SidewinderCamera, "Sidewinder camera mode", _ => "[as-swind]",
                asConfigBinding: (GameProtocol.Sidewinder, true)),
            new(SongTagToken.BankingCamera, "Banking camera mode", _ => "[as-bankcam]",
                asConfigBinding: (GameProtocol.UseBankingCamera, true)),
            new(SongTagToken.FirstPerson, "First-person camera mode", _ => "[as-first]"),
            new(SongTagToken.Caterpillar, "Big strings of blocks instead of only solo blocks", _ => "[as-caterp]"),
            new(SongTagToken.MinimumMatchSize, "Minimum match size", p => $"[as-msz{p}]", hasParameter: true, minParameter: 0, maxParameter: 24),
            new(SongTagToken.WhitesBlacksPercent, "Percentage of white/black blocks", p => $"[as-wb{p}]", hasParameter: true, minParameter: 0, maxParameter: 100),
            new(SongTagToken.MatchCollectionTicks, "Match collection ticks (default 10-25)", p => $"[as-mt{p}]", hasParameter: true, minParameter: 0),
            new(SongTagToken.PuzzleRowsCount, "Puzzle row count", p => $"[as-prows{p}]", hasParameter: true, minParameter: 0, maxParameter: 8),
            new(SongTagToken.HidePuzzleGrid, "Play with an invisible puzzle grid", _ => "[as-hidepuz]"),
            new(SongTagToken.Steep, "For slow songs, dramatically increase slope and speed of the track", _ => "[as-steep]"),
        };

        private static readonly Dictionary<SongTagToken, SongTagDefinition> _byToken =
            All.ToDictionary(t => t.Token);

        public static SongTagDefinition Get(SongTagToken token) => _byToken[token];
    }
}
