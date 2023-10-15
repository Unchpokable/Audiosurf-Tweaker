using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using MdXaml;
using Updater.Core;
using Updater.Models;

namespace Updater.ViewModels
{
    class MainViewModel : ViewModelBase
    {
        public MainViewModel()
        {
            Style = MarkdownStyle.GithubLike;

            Task.Run(async () =>
            {
                var downloader = new GithubDownloader(_user, _repo);
                ReleaseInfo = await downloader.GetLatestRelease();

                _downloadProxy = new DownloadProxy(ReleaseInfo.DownloadUrl);
                Process.Start(await _downloadProxy.StartDownloadAsync());
            });
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

        public ReleaseInfo ReleaseInfo
        {
            get => _releaseInfo;
            set
            {
                _releaseInfo = value;
                OnPropertyChanged();
            }
        }

        private Style _style;
        private ReleaseInfo _releaseInfo;
        private DownloadProxy _downloadProxy;

        private const string _user = "Unchpokable";
        private const string _repo = "Audiosurf-Tweaker";
    }
}
