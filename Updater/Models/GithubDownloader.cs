using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Updater.Core.Extensions;
using Updater.Core.Network;
using Updater.Models.GithubApiResponsesModels;

namespace Updater.Models
{
    internal class GithubDownloader
    {
        public GithubDownloader(string user, string repo)
        {
            _targetUrl = string.Format(_releasesUrlTemplate, user, repo);
        }

        private static readonly string _releasesUrlTemplate = "https://api.github.com/repos/{0}/{1}/releases";
        private string _targetUrl;
        private string _content;


        public async Task<ReleaseResponseModel?> GetLatestReleaseFull()
        {
            var status = await Get();

            if (status != HttpStatusCode.OK)
                return null;

            var releases = JsonConvert.DeserializeObject<List<ReleaseResponseModel>>(_content);
            if (releases == null)
                return null;

            ReleaseResponseModel latest = null;
            if (releases.Count > 1)
                latest = releases.First();

            return latest;
        }

        public async Task<ReleaseInfo> GetLatestRelease()
        {
            var fullRelease = await GetLatestReleaseFull();
            if (fullRelease == null)
                return null;

            return new ReleaseInfo
            {
                Body = fullRelease.body, 
                Date = fullRelease.created_at.ToString(),
                DownloadUrl = fullRelease.assets?.FirstOrNull(x => x.name == "Update.zip")?.browser_download_url
            };
        }

        private async Task<HttpStatusCode> Get()
        {
            try
            {
                using var client = new HttpClient();
                client.DefaultRequestHeaders.Add("User-Agent", "Audiosurf Tweaker");
                client.DefaultRequestHeaders.Accept.Add(new MediaTypeWithQualityHeaderValue("application/json"));
                var response = await client.GetAsync(_targetUrl);

                if (response.IsSuccessStatusCode)
                {
                    var json = await response.Content.ReadAsStringAsync();
                    _content = json;
                }

                return response.StatusCode;
            }
            catch (Exception ex)
            {
                throw new NetworkMethodFailedException($"Network method call failed with exception - {ex.Message}", ex);
            }
        }
    }
}
