using System;
using System.IO;
using System.Threading.Tasks;
using TweakerCore.Engine;
using TweakerUI.Core;

namespace TweakerUI.Models
{
    // Ported from SkinChangerRestyle.MVVM.Model.TexturesWatcher, adapted for one behavioral change:
    // the donor only ever created this at app startup (the "Enabled" toggle needed a full app restart
    // to take effect - a limitation of that constructor-only wiring, not anything intrinsic to the
    // watcher itself). SettingViewModel now creates/disposes the singleton live from the toggle
    // instead, via Instance/Reset() below.
    internal class TexturesWatcher : IDisposable
    {
        private TexturesWatcher()
        {
            _watcher = new FileSystemWatcher();
            _watcher.Changed += OnWatcherTriggered;
            _watcher.Created += OnWatcherTriggered;
            _watcher.Deleted += OnWatcherTriggered;
            _watcher.Renamed += OnWatcherTriggered;
            AllowRaisingEvents = true;
        }

        public event FileSystemEventHandler Triggered;
        public event EventHandler DiskOperationCompleted;

        public bool AllowRaisingEvents { get; set; }

        private static TexturesWatcher _instance;

        public static TexturesWatcher Instance => _instance ??= new TexturesWatcher();

        // Safe to call even when nothing turned the watcher on - InstallSkin/RemoveSkin guard their
        // own temporary-suppression calls through this instead of Instance, so they never accidentally
        // spin up a FileSystemWatcher the user never opted into.
        public static TexturesWatcher IfActive => SettingsProvider.WatcherEnabled ? Instance : null;

        public static void Reset()
        {
            _instance?.Dispose();
            _instance = null;
        }

        public string TargetPath
        {
            get => _targetPath;
            set
            {
                if (string.IsNullOrEmpty(value) || !Directory.Exists(value))
                    throw new ArgumentException("Trying to track non-existent directory");
                _targetPath = value;
                _watcher.Path = value;
                _watcher.EnableRaisingEvents = true;
            }
        }

        public string TempFilePath { get; private set; }

        private string _targetPath;
        private readonly FileSystemWatcher _watcher;
        private readonly object _lockRoot = new object();
        private DateTime _lastTrigger;

        private void OnWatcherTriggered(object sender, FileSystemEventArgs e)
        {
            if (!AllowRaisingEvents)
                return;

            if ((DateTime.Now - _lastTrigger).TotalMilliseconds <= 200) // avoid spamming on multi-file operations
                return;

            _lastTrigger = DateTime.Now;
            Triggered?.Invoke(sender, e);
        }

        public async Task InitializeTempFileAsync(string path)
        {
            if (!File.Exists(path))
            {
                Directory.CreateDirectory(Path.GetDirectoryName(path) ?? throw new InvalidOperationException());
                using (File.Create(path)) { }
            }

            TempFilePath = path;
            await OverwriteTempFile();
        }

        public Task OverwriteTempFile()
        {
            return Task.Run(() =>
            {
                lock (_lockRoot)
                {
                    using var skin = SkinPackager.CreateSkinFromFolder(SettingsProvider.GameTexturesPath);
                    if (skin == null)
                        return; // folder unreadable/missing right when the watcher fired - next trigger retries

                    SkinPackager.CompileToFile(skin, TempFilePath);
                    DiskOperationCompleted?.Invoke(this, EventArgs.Empty);
                }
            });
        }

        public void DisableRaisingEvents() => AllowRaisingEvents = false;

        public void EnableRaisingEvents() => AllowRaisingEvents = true;

        public void Dispose()
        {
            _watcher.Changed -= OnWatcherTriggered;
            _watcher.Created -= OnWatcherTriggered;
            _watcher.Deleted -= OnWatcherTriggered;
            _watcher.Renamed -= OnWatcherTriggered;
            _watcher.Dispose();
        }
    }
}
