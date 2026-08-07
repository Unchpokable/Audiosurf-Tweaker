using System;
using System.Collections.Generic;

namespace QuickPlayerCore
{
    /// <summary>
    /// The order the playlist gets played in: a list of playlist indices plus a cursor. For the linear
    /// modes it is just 0..n-1; for the shuffled ones a Fisher-Yates permutation built up front.
    ///
    /// Everything follows from that. Next is cursor + 1 and Prev is cursor - 1 in every mode, differing
    /// only in whether the ends wrap; "already played" is the part of the list left of the cursor, so
    /// shuffle needs no history stack and Prev after a shuffled Next lands back where it came from.
    ///
    /// The cursor is not stored - it is looked up from the index the caller passes in, so
    /// PlaybackController stays the only owner of "where the module is" and two positions cannot drift.
    ///
    /// Rebuilding is the caller's call, not something guessed at here. Entries only ever change because
    /// the UI changed them, and that code can say so; watching for it instead meant reconciling against
    /// a mutable collection on every single query, which was a third of this class. The one thing still
    /// checked automatically is the entry count, because a stale index would otherwise be handed to the
    /// game.
    ///
    /// Not thread-safe: every call comes from PlaybackController under its own lock, and nothing here
    /// calls back out.
    /// </summary>
    internal sealed class PlaybackOrder
    {
        public PlaybackOrder(Random random = null)
        {
            // Random.Shared unless a test wants a reproducible permutation.
            _random = random ?? Random.Shared;
        }

        private readonly Random _random;
        private readonly List<int> _order = new();

        private Guid _playlistId;
        private PlaybackMode _mode;

        /// <summary>
        /// Starts a fresh pass over <paramref name="playlist"/>, positioned at
        /// <paramref name="anchorIndex"/> (-1 for "nothing is playing"). Call it whenever the entries or
        /// the mode change, and whenever the user picks a track to play rather than letting the queue
        /// carry on.
        /// </summary>
        public void Rebuild(Playlist playlist, int anchorIndex)
        {
            _order.Clear();

            if (playlist == null)
            {
                _playlistId = Guid.Empty;
                return;
            }

            _playlistId = playlist.Id;
            _mode = playlist.Mode;

            for (var i = 0; i < playlist.Entries.Count; i++)
                _order.Add(i);

            if (!IsShuffled(_mode))
                return;

            Shuffle();

            // The pass must open on the track that is actually playing. Without this it opens wherever
            // that track happened to land in the permutation and plays only the tail after it - which is
            // exactly how Shuffle came to stop after two songs out of eight.
            MoveToFront(anchorIndex);
        }

        /// <summary>Forgets the order; the next query rebuilds it.</summary>
        private void Reset()
        {
            _order.Clear();
            _playlistId = Guid.Empty;
        }

        /// <summary>
        /// The index to play after <paramref name="currentIndex"/>, or null to stop.
        /// <paramref name="isAutoAdvance"/> separates "the track ended by itself" from a pressed Next:
        /// only the former repeats (RepeatOne) or stops (Single). A manual skip walks the order in every
        /// mode, which is what keeps the transport behaving the same everywhere.
        /// </summary>
        public int? Next(Playlist playlist, int currentIndex, bool isAutoAdvance)
        {
            EnsureBuilt(playlist, currentIndex);
            if (_order.Count == 0)
                return null;

            var cursor = _order.IndexOf(currentIndex);
            if (cursor < 0)
                return _order[0];

            if (isAutoAdvance)
            {
                if (_mode == PlaybackMode.Single)
                    return null;

                if (_mode == PlaybackMode.RepeatOne)
                    return currentIndex;
            }

            if (cursor + 1 < _order.Count)
                return _order[cursor + 1];

            if (!LoopsPlaylist(_mode))
                return null;

            // Nothing to reshuffle or wrap into.
            if (_order.Count == 1)
                return _order[0];

            if (!IsShuffled(_mode))
                return _order[0];

            // A fresh permutation for the new pass, with the track that just played parked at the front
            // so it cannot be the one that opens it.
            Shuffle();
            MoveToFront(currentIndex);
            return _order[1];
        }

        /// <summary>The index before <paramref name="currentIndex"/>, or null at the start of a non-looping order.</summary>
        public int? Previous(Playlist playlist, int currentIndex)
        {
            EnsureBuilt(playlist, currentIndex);
            if (_order.Count == 0)
                return null;

            var cursor = _order.IndexOf(currentIndex);
            if (cursor < 0)
                return _order[0];

            if (cursor > 0)
                return _order[cursor - 1];

            return LoopsPlaylist(_mode) ? _order[_order.Count - 1] : null;
        }

        /// <summary>
        /// What is queued after the current entry, nearest first; <paramref name="count"/> of 0 or less
        /// means the rest of the pass. Answers what will actually play - nothing in Single, the current
        /// track in RepeatOne.
        /// </summary>
        public IReadOnlyList<PlaylistEntry> Upcoming(Playlist playlist, int currentIndex, int count)
        {
            EnsureBuilt(playlist, currentIndex);

            var result = new List<PlaylistEntry>();
            if (_order.Count == 0 || _mode == PlaybackMode.Single)
                return result;

            var cursor = _order.IndexOf(currentIndex);

            if (_mode == PlaybackMode.RepeatOne)
            {
                if (cursor >= 0)
                    result.Add(playlist.Entries[currentIndex]);

                return result;
            }

            // Everything but the current entry is the most anyone can be given: past that a looping mode
            // just repeats itself.
            var limit = count > 0 ? Math.Min(count, _order.Count - 1) : _order.Count - 1;

            for (var step = 1; result.Count < limit; step++)
            {
                var position = cursor + step;
                if (position >= _order.Count)
                {
                    if (!LoopsPlaylist(_mode))
                        break;

                    position %= _order.Count;
                }

                result.Add(playlist.Entries[_order[position]]);
            }

            return result;
        }

        /// <summary>What has played in this pass, most recent first; <paramref name="count"/> of 0 or less means all of it.</summary>
        public IReadOnlyList<PlaylistEntry> Recent(Playlist playlist, int currentIndex, int count)
        {
            EnsureBuilt(playlist, currentIndex);

            var result = new List<PlaylistEntry>();
            var cursor = _order.IndexOf(currentIndex);
            if (cursor <= 0)
                return result;

            var limit = count > 0 ? Math.Min(count, cursor) : cursor;

            for (var position = cursor - 1; position >= 0 && result.Count < limit; position--)
                result.Add(playlist.Entries[_order[position]]);

            return result;
        }

        /// <summary>
        /// The order PlaylistPrewarmer should walk: what plays next first, then the rest. Covers every
        /// entry exactly once whatever the mode - the prewarmer prunes its temp folder against the set it
        /// prepared, and a partial list would delete copies that are still live.
        /// </summary>
        public IReadOnlyList<PlaylistEntry> PlanPrewarm(Playlist playlist, int currentIndex)
        {
            EnsureBuilt(playlist, currentIndex);

            var result = new List<PlaylistEntry>(_order.Count);
            if (_order.Count == 0)
                return result;

            var cursor = _order.IndexOf(currentIndex);

            // RepeatOne replays what is already playing, so that is the file worth having ready first.
            var offset = _mode == PlaybackMode.RepeatOne ? 0 : 1;
            var start = cursor < 0 ? 0 : cursor + offset;

            for (var step = 0; step < _order.Count; step++)
                result.Add(playlist.Entries[_order[(start + step) % _order.Count]]);

            return result;
        }

        private static bool IsShuffled(PlaybackMode mode) =>
            mode is PlaybackMode.Shuffle or PlaybackMode.ShuffleLoop;

        private static bool LoopsPlaylist(PlaybackMode mode) =>
            mode is PlaybackMode.RepeatAll or PlaybackMode.ShuffleLoop;

        // A different playlist or mode needs a new order, and so does a length that no longer matches -
        // the owner is supposed to call Rebuild when it edits the entries, and this is what stops a
        // missed call from handing an out-of-range index to the game.
        private void EnsureBuilt(Playlist playlist, int currentIndex)
        {
            if (playlist == null)
            {
                Reset();
                return;
            }

            if (_playlistId == playlist.Id && _mode == playlist.Mode && _order.Count == playlist.Entries.Count)
                return;

            Rebuild(playlist, currentIndex);
        }

        private void Shuffle()
        {
            for (var i = _order.Count - 1; i > 0; i--)
            {
                var j = _random.Next(i + 1);
                (_order[i], _order[j]) = (_order[j], _order[i]);
            }
        }

        private void MoveToFront(int index)
        {
            var position = _order.IndexOf(index);
            if (position <= 0)
                return;

            _order.RemoveAt(position);
            _order.Insert(0, index);
        }
    }
}
