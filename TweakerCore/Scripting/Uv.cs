using System;
using System.IO;
using System.Linq;

namespace TweakerCore.Scripting
{
    public enum UvState
    {
        /// <summary>On the PATH this process has - and so on the PATH of every editor Script Dev opens.</summary>
        Ready,

        /// <summary>
        /// On the user's PATH as it is now, not on the one this process started with: installed while the Tweaker was
        /// running. An editor opened from here inherits the old one, so cgrpy will not find it until a restart.
        /// </summary>
        NeedsRestart,

        Missing
    }

    /// <summary>
    /// uv, which CgrPy runs on and which is not shipped (dev-bundle-roadmap.md §2.2): the user installs it, Script Dev
    /// only says whether it is there and how to get it.
    /// </summary>
    public static class Uv
    {
        public const string InstallCommand = "winget install --id=astral-sh.uv -e";

        public static UvState Locate(string processPath, string currentUserPath, string currentMachinePath)
        {
            if (Find(processPath))
                return UvState.Ready;
            return Find(currentUserPath) || Find(currentMachinePath) ? UvState.NeedsRestart : UvState.Missing;
        }

        public static UvState LocateOnSystem() => Locate(
            Environment.GetEnvironmentVariable("PATH") ?? "",
            Environment.GetEnvironmentVariable("PATH", EnvironmentVariableTarget.User) ?? "",
            Environment.GetEnvironmentVariable("PATH", EnvironmentVariableTarget.Machine) ?? "");

        private static bool Find(string pathVariable)
        {
            foreach (var folder in (pathVariable ?? "").Split(';', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
            {
                try
                {
                    if (File.Exists(Path.Combine(Environment.ExpandEnvironmentVariables(folder.Trim('"')), "uv.exe")))
                        return true;
                }
                catch (ArgumentException)
                {
                    // a malformed PATH entry names nothing
                }
            }
            return false;
        }
    }
}
