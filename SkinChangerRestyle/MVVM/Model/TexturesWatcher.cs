using ChangerAPI.Engine;
using SkinChangerRestyle.Core;
using System;
using System.IO;
using System.Threading.Tasks;

namespace SkinChangerRestyle.MVVM.Model
{
    internal class TexturesWatcher : ObservableObject, IDisposable
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

        public static TexturesWatcher Instance
        {
            get
            {
                if (_instance == null) 
                    _instance = new TexturesWatcher();
                return _instance;
            }
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
                OnPropertyChanged();
            }
        }

        private static TexturesWatcher _instance;

        public void OnWatcherTriggered(object sender, FileSystemEventArgs e)
        {
            if (!AllowRaisingEvents) return;

            if ((DateTime.Now - _lastTrigger).TotalMilliseconds > 200 ) //Avoid massive command spam when some skin installs
            {
                _lastTrigger = DateTime.Now;
                Triggered?.Invoke(sender, e);
            }
        }

        public string TempFilePath { get; set; }
        private string _targetPath;
        private FileSystemWatcher _watcher;
        private object _lockRoot = new object();
        private DateTime _lastTrigger;

        public async void InitializeTempFile(string path)
        {
            if (!File.Exists(path))
            {
                Directory.CreateDirectory(Path.GetDirectoryName(path));
                using (var _ = File.Create(path)) { }
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
                    using (var skin = SkinPackager.CreateSkinFromFolder(SettingsProvider.GameTexturesPath))
                    {
                        SkinPackager.CompileToFile(skin, TempFilePath);
                    }
                    DiskOperationCompleted?.Invoke(this, EventArgs.Empty);
                }
            });
        }
        #region Dispose
        private bool _disposedValue;

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposedValue)
            {
                if (disposing)
                {
                    _watcher.Changed -= OnWatcherTriggered;
                    _watcher.Created -= OnWatcherTriggered;
                    _watcher.Deleted -= OnWatcherTriggered;
                    _watcher.Renamed -= OnWatcherTriggered;
                    _watcher.Dispose();
                }
                _disposedValue = true;
            }
        }

        public void Dispose()
        {
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }
        #endregion
    }
}
