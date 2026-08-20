namespace TweakerCore.FolderChecker
{
    public static class EnvironmentChecker
    {
        /// <summary>
        /// Whether the folder still holds exactly the files it held when its state was last saved.
        /// False also when there is no saved state at all - the caller treats "unknown" the same way
        /// it treats "drifted": it can't vouch for the folder either way.
        /// </summary>
        public static bool CheckEnvironment(string path, out FolderHashInfo currentState)
        {
            if (!FolderHashInfo.TryFind(path, out currentState))
                return false;

            return currentState.Equals(FolderHashInfo.Create(path));
        }
    }
}
