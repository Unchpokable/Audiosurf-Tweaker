using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace QuickPlayerCore
{
    /// <summary>How ready one playlist entry is to be handed to the game.</summary>
    public enum EntryReadiness
    {
        /// <summary>Not looked at yet, or belongs to a playlist nobody is prewarming.</summary>
        Unknown,

        /// <summary>Being copied and retagged right now.</summary>
        Preparing,

        /// <summary>The exact file the game will be given exists on disk.</summary>
        Ready,

        /// <summary>The source file is gone - moved, renamed or deleted since it was added.</summary>
        Missing
    }

    public sealed class PrewarmProgress
    {
        public PrewarmProgress(Guid entryId, EntryReadiness readiness, int settled, int total)
        {
            EntryId = entryId;
            Readiness = readiness;
            Settled = settled;
            Total = total;
        }

        /// <summary>The entry this report is about.</summary>
        public Guid EntryId { get; }

        public EntryReadiness Readiness { get; }

        /// <summary>How many entries have reached a final state (Ready or Missing).</summary>
        public int Settled { get; }

        public int Total { get; }

        public bool IsComplete => Settled >= Total;
    }

    internal interface IPlaylistPrewarmer : IDisposable
    {
        /// <summary>
        /// (Re)starts prewarming <paramref name="playlist"/> in the given order, which must cover every
        /// entry exactly once (see Run's pruning) and lead with whatever plays next.
        /// </summary>
        void Start(Playlist playlist, IReadOnlyList<PlaylistEntry> order);

        void Stop();

        EntryReadiness GetReadiness(PlaylistEntry entry);
    }

    /// <summary>
    /// Walks a playing playlist in the background and gets every entry ready before the player
    /// actually needs it: checks the source file still exists, and materialises the tagged temp copy
    /// TempFileTagger would otherwise build at the last moment, right when the game is waiting for a
    /// playsong.
    ///
    /// The order comes from the caller (PlaybackOrder.PlanPrewarm) rather than being derived from an
    /// index here. Deciding it locally only works while "next" means "next index" - under a shuffled
    /// mode it would prepare the entry's neighbour in the list while a completely different track is
    /// the one about to play, which is exactly the wait this class exists to remove.
    ///
    /// Sequential on one background task on purpose. The goal is "the next track is ready in time",
    /// not "the whole playlist is ready as fast as possible" - copying several multi-megabyte files at
    /// once would compete for the same disk and make the one entry that actually matters land later.
    ///
    /// Preparation is idempotent and content-addressed (see TempFileTagger), so this racing the
    /// player's own resolve of the same entry costs at worst a duplicated copy, never a corrupt file.
    ///
    /// Assumes the user does not edit the source files while their playlist is playing. Detecting that
    /// would mean watching every file for changes and re-preparing mid-playback; the trade isn't worth
    /// it, and the failure mode is a track playing with the tags it had when playback started.
    /// </summary>
    internal sealed class PlaylistPrewarmer : IPlaylistPrewarmer
    {
        public PlaylistPrewarmer(IProgress<PrewarmProgress> progress = null)
        {
            _progress = progress;
        }

        private readonly IProgress<PrewarmProgress> _progress;
        private readonly object _gate = new();
        private readonly Dictionary<Guid, EntryReadiness> _readiness = new();

        private CancellationTokenSource _cancellation;
        private Task _worker;
        private Guid _playlistId;

        public void Start(Playlist playlist, IReadOnlyList<PlaylistEntry> order)
        {
            if (playlist == null || order == null)
                return;

            CancellationTokenSource cancellation;

            lock (_gate)
            {
                // A pass already covering this playlist keeps going - restarting on every track change
                // would throw away everything it has prepared and begin again from the new position.
                // A mode change reorders what is still to come but not what is already on disk: the temp
                // copies are content-addressed, so anything prepared stays a cache hit regardless.
                if (_playlistId == playlist.Id && _worker != null && !_worker.IsCompleted)
                    return;

                CancelLocked();
                _readiness.Clear();
                _playlistId = playlist.Id;
                _cancellation = cancellation = new CancellationTokenSource();
            }

            // Copied because the caller's list may be a live view, and this one is walked from a
            // background thread for as long as the pass runs.
            var plan = new List<PlaylistEntry>(order);
            var token = cancellation.Token;

            var worker = Task.Run(() => Run(playlist.Id, plan, token), token);

            lock (_gate)
            {
                if (_cancellation == cancellation)
                    _worker = worker;
            }
        }

        public void Stop()
        {
            lock (_gate)
            {
                CancelLocked();
                _readiness.Clear();
                _playlistId = Guid.Empty;
            }
        }

        public EntryReadiness GetReadiness(PlaylistEntry entry)
        {
            if (entry == null)
                return EntryReadiness.Unknown;

            lock (_gate)
                return _readiness.TryGetValue(entry.Id, out var readiness) ? readiness : EntryReadiness.Unknown;
        }

        public void Dispose()
        {
            lock (_gate)
                CancelLocked();
        }

        private void CancelLocked()
        {
            _cancellation?.Cancel();
            _cancellation?.Dispose();
            _cancellation = null;
            _worker = null;
        }

        private void Run(Guid playlistId, List<PlaylistEntry> order, CancellationToken token)
        {
            var keep = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var settled = 0;

            foreach (var entry in order)
            {
                if (token.IsCancellationRequested)
                    return;

                if (entry == null)
                {
                    settled++;
                    continue;
                }

                if (string.IsNullOrEmpty(entry.FilePath) || !File.Exists(entry.FilePath))
                {
                    settled++;
                    Publish(entry.Id, EntryReadiness.Missing, settled, order.Count, token);
                    continue;
                }

                Publish(entry.Id, EntryReadiness.Preparing, settled, order.Count, token);

                // ResolvePlaybackPath reports its own failures through TempFileTagger.OperationFailed
                // and degrades to the original file, so a failed retag still leaves something playable.
                var prepared = TempFileTagger.ResolvePlaybackPath(playlistId, entry);
                keep.Add(prepared);

                settled++;
                Publish(entry.Id, EntryReadiness.Ready, settled, order.Count, token);
            }

            if (token.IsCancellationRequested)
                return;

            // Only after a complete pass: anything left over belongs to entries that were removed, or
            // to copies whose source file or tag set has changed since. Pruning partway through would
            // delete files for entries this pass simply hasn't reached yet.
            TempFileTagger.PruneTempDirectory(playlistId, keep);
        }

        private void Publish(Guid entryId, EntryReadiness readiness, int settled, int total, CancellationToken token)
        {
            lock (_gate)
            {
                if (token.IsCancellationRequested)
                    return;

                _readiness[entryId] = readiness;
            }

            _progress?.Report(new PrewarmProgress(entryId, readiness, settled, total));
        }
    }
}
