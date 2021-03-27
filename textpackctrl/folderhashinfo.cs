namespace FolderChecker
{
    using System;
    using System.Security.Cryptography;
    using System.Collections.Generic;
    using System.Linq;
    using System.Runtime.Serialization;
    using System.Runtime.Serialization.Formatters.Binary;
    using System.Text;
    using System.IO;
    using FolderChecker.Exceptions;

    [Serializable]
    public class FolderHashInfo : IEquatable<FolderHashInfo>
    {
        public string StateName { get; set; }
        public string FolderName { get; set; }
        public string Location { get; set; }

        public IList<byte[]> ContainedFilesHashes;

        private static readonly string stdExt = ".hinf";

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
            path = path + @"\current" + stdExt;
            using (var fileStream = new FileStream(path, FileMode.Create))
            {
                var formatter = new BinaryFormatter();
                formatter.Serialize(fileStream, this);
            }
        }

        public static bool TryFind(string path, out FolderHashInfo folderInfo)
        {
            var isOk = TryFind(path, stdExt, out FolderHashInfo result);
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
            var containedFiles = Directory.EnumerateFiles(path);
            foreach (var file in containedFiles)
            {
                if (Path.GetExtension(file) == specificExtension)
                {
                    try
                    {
                        using (var fileStream = new FileStream(file, FileMode.Open))
                        {
                            var formatter = new BinaryFormatter();
                            folderInfo = (FolderHashInfo)formatter.Deserialize(fileStream);
                            return true;
                        }
                    }
                    catch (Exception e)
                    {
                        throw new BadFileFormatException($"File {Path.GetFileName(file)} is marked as FolderHashInfo serialized data, but contains an invalid binary data format for this type");
                    }
                }
            }
            folderInfo = null;
            return false;
        }

        public static FolderHashInfo Find(string path)
        {
            return Find(path, stdExt);
        }

        public static FolderHashInfo Find(string path, string specificExtension)
        {
            var containedFiles = Directory.EnumerateFiles(path);
            foreach (var file in containedFiles)
            {
                if (Path.GetExtension(file) == specificExtension)
                {
                    try
                    {
                        using (var fileStream = new FileStream(file, FileMode.Open))
                        {
                            var formatter = new BinaryFormatter();
                            return (FolderHashInfo)formatter.Deserialize(fileStream);
                        }
                    }
                    catch (Exception e)
                    {
                        throw new BadFileFormatException($"File {Path.GetFileName(file)} is marked as FolderHashInfo serialized data, but contains an invalid binary data format for this type");
                    }
                }
            }
            return null;
        }

        public static FolderHashInfo Create(string path)
        {
            return Create(path, "defaul");
        }

        public static FolderHashInfo Create(string path, string stateName)
        {
            var containedFiles = Directory.EnumerateFiles(path);
            var hashes = new List<byte[]>();

            using (var hashProvider = SHA256.Create())
                foreach (var file in containedFiles)
                {
                    if (Path.GetExtension(file) == stdExt) continue;
                    using (var stream = File.OpenRead(file))
                        hashes.Add(hashProvider.ComputeHash(stream));
                }
            return new FolderHashInfo(path, stateName) { ContainedFilesHashes = hashes };
        }
    }
}
