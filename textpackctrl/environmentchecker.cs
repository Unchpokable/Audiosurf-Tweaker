namespace FolderChecker
{
    using System.Runtime.Serialization.Formatters.Binary;
    using System.Runtime.Serialization;
    using System.IO;

    public class EnvironmentChecker
    {
        public static bool CheckEnvironment(FolderHashInfo info)
        {
            var actualInfo = FolderHashInfo.Create(info.Location);
            return info.Equals(actualInfo);
        }

        public static bool CheckEnvironment(string path, out FolderHashInfo currentState)
        {
            currentState = null;
            if (!FolderHashInfo.TryFind(path, out FolderHashInfo savedInfo))
                return false;
            var actualInfo = FolderHashInfo.Create(path);
            currentState = savedInfo;
            return savedInfo.Equals(actualInfo);
        }

        public static void SaveState(string path)
        {
            SaveState(path, "default");
        }

        public static void SaveState(string path, string stateName)
        {
            var state = FolderHashInfo.Create(path, stateName);
            state.Save(path);
        }
    }
}
