using System;
using Updater.Core;

namespace Updater.Models
{
    class ReleaseInfo : ViewModelBase
    {
        public ReleaseInfo() { }

        public string Date
        {
            get => _releaseDate.ToString();
            set
            {
                _releaseDate = DateTime.Parse(value);
                OnPropertyChanged();
            }
        }

        public string Body
        {
            get => _body;
            set
            {
                _body = value;
                OnPropertyChanged();
            }
        }

        public string DownloadUrl
        {
            get => _updateDownloadPath;
            set
            {
                _updateDownloadPath = value;
                OnPropertyChanged();
            }
        }

        private DateTime _releaseDate;
        private string _body;
        private string _updateDownloadPath;
    }
}
