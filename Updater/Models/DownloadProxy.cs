using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Threading;
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

        /// <summary>
        /// Starts downloading file by given URL and returns path to downloaded file
        /// </summary>
        /// <param name="url"></param>
        /// <returns></returns>
        /// <exception cref="ArgumentException">Throws if object was initialized with null url and StartDownloadAsync was called without parameter</exception>
        public async Task<string> StartDownloadAsync(string url = null)
        {
            if (string.IsNullOrEmpty(url) && string.IsNullOrEmpty(_downloadUrl))
                throw new ArgumentException("Target download url was null when download was tried to start");

            if (url != null)
                _downloadUrl = url;

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

                var progress = new Progress<double>(percent =>
                {
                    DownloadedSize = totalBytesRead;
                    DownloadSpeed = percent;
                });

                while ((bytesRead = await download.ReadAsync(buffer, 0, buffer.Length)) > 0)
                {
                    await fileStream.WriteAsync(buffer, 0, bytesRead);
                    totalBytesRead += bytesRead;
                    ((IProgress<double>)progress).Report((double)totalBytesRead / TotalSize);
                }

                return fileStream.Name;
            }

            return null;
        }
    }
}
