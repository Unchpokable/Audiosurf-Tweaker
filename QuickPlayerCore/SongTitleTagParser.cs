using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using QuickPlayerCore.Audiosurf;

namespace QuickPlayerCore
{
    /// <summary>A song title split into its plain text and the Audiosurf tags that were written into it.</summary>
    public sealed class ParsedSongTitle
    {
        internal ParsedSongTitle(string title, IReadOnlyList<PlaylistTag> tags)
        {
            Title = title;
            Tags = tags;
        }

        /// <summary>The title with every recognised tag removed and leftover whitespace collapsed.</summary>
        public string Title { get; }

        /// <summary>Recognised tags in the order they appeared, one per token (a repeated token keeps its first parameter).</summary>
        public IReadOnlyList<PlaylistTag> Tags { get; }
    }

    /// <summary>
    /// Reads the bracket tags Audiosurf understands back out of a song title - the inverse of
    /// SongTagDefinition.Format, which is the only writer of them (TempFileTagger).
    ///
    /// This exists because the game is hostile to malformed input: a title carrying the same tag twice
    /// (the user wrote it into the file themselves, then enabled the same tag in Quick Player, which
    /// appended a second copy) does not get ignored - it breaks the game in arbitrary ways, verified on
    /// a real run. Lifting the tags out of the title at import means the title Quick Player stores is
    /// plain text and the tag set is data, so composing them back together cannot produce a duplicate.
    ///
    /// Deliberately conservative: anything that is not an exact match for what Format would have
    /// produced is left in the title untouched. Bracketed text is ordinary in song names ("[Remix]",
    /// "[Live]"), a differently-cased or misspelled tag is not something the game would have honoured
    /// either, and silently rewriting the user's title on a guess is worse than leaving it alone.
    /// </summary>
    public static class SongTitleTagParser
    {
        public static ParsedSongTitle Parse(string title)
        {
            if (string.IsNullOrEmpty(title) || title.IndexOf('[') < 0)
                return new ParsedSongTitle(title ?? string.Empty, Array.Empty<PlaylistTag>());

            var tags = new List<PlaylistTag>();
            var seen = new HashSet<SongTagToken>();
            var kept = new StringBuilder(title.Length);
            var position = 0;

            while (position < title.Length)
            {
                var open = title.IndexOf('[', position);
                if (open < 0)
                    break;

                var close = title.IndexOf(']', open + 1);
                if (close < 0)
                    break;

                var group = title.Substring(open, close - open + 1);
                var tag = Match(group);
                if (tag == null)
                {
                    // Not one of ours - keep it, brackets and all, and resume scanning after it in case
                    // a real tag follows.
                    kept.Append(title, position, close - position + 1);
                    position = close + 1;
                    continue;
                }

                if (seen.Add(tag.Token))
                    tags.Add(tag);

                kept.Append(title, position, open - position);
                position = close + 1;
            }

            if (position < title.Length)
                kept.Append(title, position, title.Length - position);

            return new ParsedSongTitle(Collapse(kept.ToString()), tags);
        }

        private static PlaylistTag Match(string group)
        {
            foreach (var definition in SongTagCatalog.All)
            {
                if (definition.HasParameter)
                    continue;

                if (string.Equals(group, definition.Format(), StringComparison.Ordinal))
                    return new PlaylistTag { Token = definition.Token };
            }

            foreach (var (definition, prefix, suffix) in _parameterized)
            {
                if (group.Length <= prefix.Length + suffix.Length)
                    continue;
                if (!group.StartsWith(prefix, StringComparison.Ordinal) || !group.EndsWith(suffix, StringComparison.Ordinal))
                    continue;

                var digits = group.Substring(prefix.Length, group.Length - prefix.Length - suffix.Length);
                if (!int.TryParse(digits, out var parameter))
                    continue;
                if (parameter < definition.MinParameter || parameter > definition.MaxParameter)
                    continue;

                // Round-trip against the formatter rather than trusting the affixes derived below - if
                // that derivation is ever wrong, this rejects the match instead of mangling a title.
                if (!string.Equals(group, definition.Format(parameter), StringComparison.Ordinal))
                    continue;

                return new PlaylistTag { Token = definition.Token, Parameter = parameter };
            }

            return null;
        }

        // Removing a tag leaves the spaces that surrounded it behind; without this, stripping tags from
        // "Song [as-4lane] [as-portal]" would store "Song  " and every later compose would carry the
        // padding forward.
        private static string Collapse(string value)
        {
            var builder = new StringBuilder(value.Length);
            var pendingSpace = false;

            foreach (var c in value)
            {
                if (char.IsWhiteSpace(c))
                {
                    pendingSpace = builder.Length > 0;
                    continue;
                }

                if (pendingSpace)
                    builder.Append(' ');
                pendingSpace = false;
                builder.Append(c);
            }

            return builder.ToString();
        }

        // The literal text around a parameterized tag's number, derived from the formatter itself
        // rather than written out a second time here - a hand-copied "[as-msz" would be free to drift
        // away from SongTagCatalog the moment a tag is renamed. Rendering the same tag with two
        // adjacent values leaves the number as the only difference, so what the two renders share is
        // exactly the fixed text.
        private static readonly (SongTagDefinition Definition, string Prefix, string Suffix)[] _parameterized =
            SongTagCatalog.All
                .Where(definition => definition.HasParameter && definition.MinParameter < definition.MaxParameter)
                .Select(definition =>
                {
                    var low = definition.Format(definition.MinParameter);
                    var high = definition.Format(definition.MinParameter + 1);

                    var prefix = 0;
                    while (prefix < low.Length && prefix < high.Length && low[prefix] == high[prefix])
                        prefix++;

                    var suffix = 0;
                    while (suffix < low.Length - prefix
                        && suffix < high.Length - prefix
                        && low[low.Length - suffix - 1] == high[high.Length - suffix - 1])
                    {
                        suffix++;
                    }

                    return (definition, low.Substring(0, prefix), low.Substring(low.Length - suffix));
                })
                .ToArray();
    }
}
