using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Threading;

namespace TweakerUI.Core
{
    /// <summary>
    /// "... on save" for a developer tab (Docs/Internal/script-manager.md §6, §8.16): watches one project folder and,
    /// once its files have been quiet for <c>settle</c>, runs the tab's work - a rebuild in Script Dev, a repack and
    /// install in Skyboxes. Runs whether the tab is on screen or not: the user switched it on and is looking at their
    /// editor, not at Tweaker.
    ///
    /// - Not a change: anything under the ignored top-level folders (output, editor state, tool files Tweaker itself
    ///   writes), and "a folder changed" - reading the files in it raises that, so the work itself would trigger it.
    /// - One save is often several writes: each change restarts the quiet time, and only the newest one runs.
    /// - Changes while the work runs - the watcher's own run or one started by hand, reported through
    ///   <see cref="WorkStarted"/>/<see cref="WorkFinished"/> - make exactly one run after it.
    ///
    /// The quiet time is a <c>Task.Delay</c> with a generation number, not a DispatcherTimer: the timer needs the
    /// platform's clock, which the headless UiStand does not run. Everything here is on the UI thread; the
    /// FileSystemWatcher's events are posted to it.
    /// </summary>
    internal sealed class ProjectWatcher : IDisposable
    {
        private readonly string _owner;
        private readonly HashSet<string> _ignored;
        private readonly TimeSpan _settle;
        private readonly Func<Task> _onSettled;

        private FileSystemWatcher _watcher;
        private bool _working;
        private bool _pending;

        /// <summary>Bumped by every change and by stopping; a delayed run that finds it moved on does nothing.</summary>
        private int _generation;

        /// <param name="owner">The tab, for the log.</param>
        /// <param name="ignoredFolders">Top-level folders of the project that are not its sources.</param>
        /// <param name="settle">Quiet time after the last change before the work runs.</param>
        /// <param name="onSettled">The work; called on the UI thread.</param>
        public ProjectWatcher(string owner, IEnumerable<string> ignoredFolders, TimeSpan settle, Func<Task> onSettled)
        {
            _owner = owner;
            _ignored = new HashSet<string>(ignoredFolders, StringComparer.OrdinalIgnoreCase);
            _settle = settle;
            _onSettled = onSettled;
        }

        /// <summary>Watches <paramref name="root"/> instead of whatever was watched before. A folder that is not there is not watched.</summary>
        public void Start(string root)
        {
            Stop();
            if (string.IsNullOrEmpty(root) || !Directory.Exists(root))
                return;

            try
            {
                _watcher = new FileSystemWatcher(root)
                {
                    IncludeSubdirectories = true,
                    NotifyFilter = NotifyFilters.FileName | NotifyFilters.DirectoryName | NotifyFilters.LastWrite | NotifyFilters.Size
                };
                void OnChange(string path)
                {
                    if (!Ignored(root, path))
                        Dispatcher.UIThread.Post(Restart);
                }
                // "Changed" on a folder is its metadata - reading the files in it is enough to raise one. What changed
                // inside arrives as events of its own. (twllink's own --watch learned this the hard way.)
                _watcher.Changed += (_, e) =>
                {
                    if (!Directory.Exists(e.FullPath))
                        OnChange(e.FullPath);
                };
                _watcher.Created += (_, e) => OnChange(e.FullPath);
                _watcher.Deleted += (_, e) => OnChange(e.FullPath);
                _watcher.Renamed += (_, e) => OnChange(e.FullPath);
                _watcher.EnableRaisingEvents = true;
            }
            catch (Exception exception) when (exception is IOException || exception is ArgumentException || exception is UnauthorizedAccessException)
            {
                Logger.Log(_owner, "cannot watch " + root + ": " + exception.Message);
                Stop();
            }
        }

        /// <summary>Stops watching and forgets any run that was waiting.</summary>
        public void Stop()
        {
            _generation++;
            _pending = false;
            _watcher?.Dispose();
            _watcher = null;
        }

        /// <summary>The tab's work began - this watcher's run or one started by hand. Changes from now wait for it.</summary>
        public void WorkStarted() => _working = true;

        /// <summary>The work ended: changes that arrived during it are one more run.</summary>
        public void WorkFinished()
        {
            _working = false;
            if (_pending)
            {
                _pending = false;
                Restart();
            }
        }

        public void Dispose() => Stop();

        private bool Ignored(string root, string path)
        {
            var relative = Path.GetRelativePath(root, path);
            var first = relative.Split(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)[0];
            return _ignored.Contains(first);
        }

        private void Restart()
        {
            if (_working)
            {
                _pending = true;
                return;
            }

            // A delay per change, and only the newest one runs: the quiet time without a timer to restart.
            var generation = ++_generation;
            _ = Task.Delay(_settle).ContinueWith(_ => Dispatcher.UIThread.Post(() =>
            {
                if (generation == _generation)
                    _ = _onSettled();
            }), TaskScheduler.Default);
        }
    }
}
