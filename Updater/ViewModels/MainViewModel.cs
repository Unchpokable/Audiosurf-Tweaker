using System;
using System.Diagnostics;
using System.Threading.Tasks;
using System.Windows;
using MdXaml;
using Updater.Core;
using Updater.Models;

namespace Updater.ViewModels
{
    class MainViewModel : ViewModelBase, IDisposable
    {
        public MainViewModel()
        {
            Style = MarkdownStyle.GithubLike;
            CurrentVisibleComponent = ComponentVisibility.None;

            Task.Run(async () =>
            {
                var githubRepo = new GithubRepoReleaseProvider(_user, _repo);
                ReleaseInfo = await githubRepo.GetLatestRelease();

                if (ReleaseInfo == null) return;

                _downloadProxy = new DownloadProxy(ReleaseInfo.DownloadUrl);
                _downloadProxy.PropertyChanged += (o, e) => OnPropertyChanged(nameof(DownloadProxy));
                CurrentVisibleComponent = ComponentVisibility.AskForDownload;
            });


            CloseApplication = new RelayCommand(o => Application.Current.Shutdown(0));
            DownloadUpdate = new RelayCommand(DownloadUpdateInternal);
        }

        public Style Style
        {
            get => _style;
            set
            {
                _style = value;
                OnPropertyChanged();
            }
        }

        public ReleaseInfo? ReleaseInfo
        {
            get => _releaseInfo;
            set
            {
                _releaseInfo = value;
                OnPropertyChanged();
            }
        }

        public DownloadProxy? DownloadProxy
        {
            get => _downloadProxy;
            set
            {
                _downloadProxy = value;
                OnPropertyChanged();
            }
        }

        public ComponentVisibility CurrentVisibleComponent
        {
            get => _currentComponentVisibility;
            set
            {
                _currentComponentVisibility = value;
                OnPropertyChanged();
            }
        }

        public RelayCommand CloseApplication { get; set; }
        public RelayCommand DownloadUpdate { get; set; }

        private Style _style;
        private ReleaseInfo? _releaseInfo;
        private DownloadProxy? _downloadProxy;
        private ComponentVisibility _currentComponentVisibility;

        private Core.Updater _updater;
        
        private const string _user = "Unchpokable";
        private const string _repo = "Audiosurf-Tweaker";

        public void Dispose()
        {
            _updater.Dispose();
        }

        public async void DownloadUpdateInternal(object _)
        {
            CurrentVisibleComponent = ComponentVisibility.DownloadingPage;
            var updateZip = await _downloadProxy.StartDownloadAsync();
        }
    }
}
