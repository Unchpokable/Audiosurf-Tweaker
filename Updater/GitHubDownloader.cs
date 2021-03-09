namespace Updater
{
    using System.Net;
    using System.Configuration;
    using System.Net.Http.Headers;
    using System.Net.Http;
    using System;
    using System.IO;

    class GitHubDownloader
    {
        private void VerifyAPIRessponse(HttpStatusCode response)
        {
            switch (response)
            {
                case HttpStatusCode.Forbidden:
                    throw new Exception("Forbidden");
                case HttpStatusCode.Unauthorized:
                    throw new Exception("Unauthorized aplication");
                case HttpStatusCode.BadRequest:
                    throw new Exception("Bad Request");
                case HttpStatusCode.NotFound:
                    throw new Exception("Requested page not founded");
                default:
                {
                    if (response != HttpStatusCode.OK)
                        throw new Exception("API Call failture");
                    return;
                }
            }
        }

        public async void DownloadRelease(string repoUrl)
        {
            string latestReleaseUrl = repoUrl + "/releases/latest";
            WebClient client = new WebClient();
            HttpWebResponse response = (HttpWebResponse)WebRequest.Create(latestReleaseUrl).GetResponse();
            VerifyAPIRessponse(response.StatusCode);
            string truelatestReleaseUrl = response.ResponseUri.AbsoluteUri.Replace("tag", "download");
            try
            {
                await client.DownloadFileTaskAsync(truelatestReleaseUrl + "/Update.zip", Path.GetTempPath() + @"\skinchanger\Update.zip");
            }
            catch (Exception e)
            {
                throw new Exception($"Unable to download Update.exe for latest awaiable GitHub Release. Original exception message: {e}");
            }
        }
    }
}
