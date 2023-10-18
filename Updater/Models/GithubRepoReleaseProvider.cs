using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Threading.Tasks;
using Newtonsoft.Json;
using Updater.Core.Extensions;
using Updater.Core.Network;
using Updater.Models.GithubApiResponsesModels;

namespace Updater.Models
{
    internal class GithubRepoReleaseProvider
    {
        public GithubRepoReleaseProvider(string user, string repo)
        {
            _targetUrl = string.Format(_releasesUrlTemplate, user, repo);
        }

        private static readonly string _releasesUrlTemplate = "https://api.github.com/repos/{0}/{1}/releases";
        private string _targetUrl;
        private string _content;

        public async Task<ReleaseResponseModel?> GetLatestReleaseFull()
        {
            var releases = await GetRepoReleases();

            if (releases == null)
                return null;

            ReleaseResponseModel latest = null;
            if (releases.Count > 1)
                latest = releases.First();

            return latest;
        }

        public async Task<ReleaseInfo?> GetLatestRelease()
        {
            var fullRelease = await GetLatestReleaseFull();
            if (fullRelease == null)
                return null;

            var asset = fullRelease.assets?.FirstOrDefault(x => x.name == "Update.zip");

            return new ReleaseInfo
            {
                Body = fullRelease.body, 
                Date = fullRelease.created_at.ToString(),
                DownloadUrl = asset?.browser_download_url,
                Title = fullRelease?.name,
                Size = asset?.size ?? -1
            };
        }

        public async Task<List<ReleaseResponseModel>?> GetRepoReleases()
        {
            var status = await Get();

            if (status != HttpStatusCode.OK)
                return null;

            var releases = JsonConvert.DeserializeObject<List<ReleaseResponseModel>>(_content);
            if (releases == null)
                return null;

            return releases;
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
