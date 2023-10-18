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

        public string Title
        {
            get => _title;
            set
            {
                _title = value;
                OnPropertyChanged();
            }
        }


        public long Size
        {
            get => _size;
            set
            {
                _size = value;
                OnPropertyChanged();
            }
        }


        private DateTime _releaseDate;
        private string _body;
        private string _updateDownloadPath;
        private string _title;
        private long _size;
    }
}
