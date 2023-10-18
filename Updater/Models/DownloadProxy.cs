using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Threading.Tasks;
using Updater.Core;

namespace Updater.Models
{
    class DownloadProxy : ViewModelBase
    {
        public DownloadProxy(string url)
        {
            _downloadUrl = url;
        }

        public long TotalSize
        {
            get => _totalSize;
            set
            {
                _totalSize = value;
                OnPropertyChanged();
            }
        }

        public long DownloadedSize
        {
            get => _downloadedSize;
            set
            {
                _downloadedSize = value;
                OnPropertyChanged();
            }
        }

        public double DownloadSpeed
        {
            get => _downloadSpeed;
            set
            {
                _downloadSpeed = value;
                OnPropertyChanged();
            }
        }

        private string _downloadUrl;
        private long _totalSize;
        private long _downloadedSize;
        private double _downloadSpeed;

        private int _defaultBufferSize = 8192;

        public async Task<string> StartDownloadAsync()
        {
            if (_downloadUrl == null)
                throw new Exception("DownloadProxy object was initialized with null-string download url");

            using var httpClient = new HttpClient(new HttpClientHandler());

            var request = new HttpRequestMessage(HttpMethod.Get, _downloadUrl);
            var response = await httpClient.SendAsync(request, HttpCompletionOption.ResponseHeadersRead);

            if (response.IsSuccessStatusCode || response.StatusCode == HttpStatusCode.Redirect)
            {
                using var fileStream = File.Create(Path.Combine(Path.GetTempPath(), Path.Combine(Path.GetTempPath(), "Update.zip")));
                using var download = await response.Content.ReadAsStreamAsync();

                TotalSize = response.Content.Headers.ContentLength ?? -1;

                var buffer = new byte[_defaultBufferSize];
                int bytesRead;
                long totalBytesRead = 0;

                while ((bytesRead = await download.ReadAsync(buffer, 0, buffer.Length)) > 0)
                {
                    await fileStream.WriteAsync(buffer, 0, bytesRead);
                    totalBytesRead += bytesRead;
                    DownloadedSize = totalBytesRead;
                }

                return fileStream.Name;
            }

            return null;
        }
    }
}
