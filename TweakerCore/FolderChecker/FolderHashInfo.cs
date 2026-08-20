using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text.Json;

namespace TweakerCore.FolderChecker
{
    public class FolderHashInfo : IEquatable<FolderHashInfo>
    {
        public string StateName { get; set; }
        public string FolderName { get; set; }
        public string Location { get; set; }

        public IList<byte[]> ContainedFilesHashes { get; set; }

        private const string StateFileExtension = ".hinf";
        private const string StateFileName = "current" + StateFileExtension;

        // Used by System.Text.Json when reading a saved cache entry back.
        public FolderHashInfo()
        {
        }

        public FolderHashInfo(string location, string stateName)
        {
            FolderName = Path.GetDirectoryName(location);
            Location = location;
            StateName = stateName;
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

        public override bool Equals(object obj)
        {
            return Equals(obj as FolderHashInfo);
        }

        // Equals compares hash *contents* and ignores their order, so the hash code has to be
        // order-independent as well: per-array content hashes XOR-ed together. A null hash list
        // never compares equal to anything (see Equals above), so its hash code is irrelevant.
        public override int GetHashCode()
        {
            if (ContainedFilesHashes == null)
                return 0;

            var accumulator = ContainedFilesHashes.Count;
            foreach (var hash in ContainedFilesHashes)
            {
                if (hash == null)
                    continue;

                var content = new HashCode();
                content.AddBytes(hash);
                accumulator ^= content.ToHashCode();
            }

            return accumulator;
        }

        public void Save(string path)
        {
            File.WriteAllText(Path.Combine(path, StateFileName), JsonSerializer.Serialize(this));
        }

        // Local cache only - if the file at this path predates the JSON format (or is otherwise
        // unreadable), it's simply treated as "no saved state" rather than converted; the caller
        // recomputes and overwrites it via Create()/Save().
        public static bool TryFind(string path, out FolderHashInfo folderInfo)
        {
            folderInfo = null;

            var stateFile = Path.Combine(path, StateFileName);
            if (!File.Exists(stateFile))
                return false;

            try
            {
                folderInfo = JsonSerializer.Deserialize<FolderHashInfo>(File.ReadAllText(stateFile));
            }
            catch (JsonException)
            {
                return false;
            }

            return folderInfo != null;
        }

        public static FolderHashInfo Create(string path, string stateName = "default")
        {
            var hashes = new List<byte[]>();

            foreach (var file in Directory.EnumerateFiles(path))
            {
                if (Path.GetExtension(file) == StateFileExtension)
                    continue;

                using (var stream = File.OpenRead(file))
                    hashes.Add(SHA256.HashData(stream));
            }

            return new FolderHashInfo(path, stateName) { ContainedFilesHashes = hashes };
        }
    }
}
