using System;
using System.Security.Cryptography;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.IO;

namespace TweakerCore.FolderChecker
{
    public class FolderHashInfo : IEquatable<FolderHashInfo>
    {
        public string StateName { get; set; }
        public string FolderName { get; set; }
        public string Location { get; set; }

        public IList<byte[]> ContainedFilesHashes { get; set; }

        private static readonly string _stdExt = ".hinf";

        // Used by System.Text.Json when reading a saved cache entry back.
        public FolderHashInfo()
        {
        }

        public FolderHashInfo(string location)
        {
            FolderName = Path.GetDirectoryName(location);
            Location = location;

        }

        public FolderHashInfo(string location, string statneName) : this(location)
        {
            StateName = statneName;
        }

        public bool Equals(FolderHashInfo obj)
        {
            if (obj == null || ContainedFilesHashes == null || obj.ContainedFilesHashes == null)
                return false;
            if (ContainedFilesHashes.Count != obj.ContainedFilesHashes.Count)
                return false;
            foreach (var array in obj.ContainedFilesHashes)
            {
                if (!ContainedFilesHashes.Any(x => x.SequenceEqual(array)))
                    return false;
            }
            return true;
        }

        public override string ToString()
        {
            var stringBuilder = new StringBuilder();
            stringBuilder.Append($"Folder: {FolderName}\n");
            stringBuilder.Append($"Absolute Path: {Location}\n");
            stringBuilder.Append("Containment files Checksums:\n");
            foreach (var hash in ContainedFilesHashes)
            {
                stringBuilder.Append($"[{hash}]\n");
            }
            return stringBuilder.ToString();
        }

        public void Save(string path)
        {
            path = path + @"\current" + _stdExt;
            File.WriteAllText(path, JsonSerializer.Serialize(this));
        }

        // Local cache only - if a file at this path predates the JSON format (or is otherwise
        // unreadable), it's simply treated as "no saved state" rather than converted; the caller
        // recomputes and overwrites it via Create()/Save().
        public static bool TryFind(string path, out FolderHashInfo folderInfo)
        {
            var isOk = TryFind(path, _stdExt, out FolderHashInfo result);
            if (isOk)
            {
                folderInfo = result;
                return true;
            }
            folderInfo = null;
            return false;
        }

        public static bool TryFind(string path, string specificExtension, out FolderHashInfo folderInfo)
        {
            folderInfo = Find(path, specificExtension);
            return folderInfo != null;
        }

        public static FolderHashInfo Find(string path)
        {
            return Find(path, _stdExt);
        }

        public static FolderHashInfo Find(string path, string specificExtension)
        {
            if (!Directory.Exists(path))
            {
                return null;
            }
            var containedFiles = Directory.EnumerateFiles(path);
            foreach (var file in containedFiles)
            {
                if (Path.GetExtension(file) == specificExtension)
                {
                    try
                    {
                        return JsonSerializer.Deserialize<FolderHashInfo>(File.ReadAllText(file));
                    }
                    catch (JsonException)
                    {
                        return null;
                    }
                }
            }
            return null;
        }

        public static FolderHashInfo Create(string path)
        {
            return Create(path, "default");
        }

        public static FolderHashInfo Create(string path, string stateName)
        {
            var containedFiles = Directory.EnumerateFiles(path);
            var hashes = new List<byte[]>();

            using (var hashProvider = SHA256.Create())
                foreach (var file in containedFiles)
                {
                    if (Path.GetExtension(file) == _stdExt) continue;
                    using (var stream = File.OpenRead(file))
                        hashes.Add(hashProvider.ComputeHash(stream));
                }
            return new FolderHashInfo(path, stateName) { ContainedFilesHashes = hashes };
        }
    }
}
