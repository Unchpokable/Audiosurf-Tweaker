using System.IO;
using TweakerCore.Scripting;

namespace TweakerUI.Core
{
    /// <summary>
    /// &lt;TweakerUI&gt;\DevTools\ - the script developer's side of the app (Docs/Internal/script-manager-roadmap.md
    /// §5, п. 1 and 3). Next to the exe like the rest of the app's data (AppPaths), not in the game:
    ///
    ///     DevTools\Binaries\twllink.exe       the linker; Native AOT in a release, a plain build in a dev one
    ///     DevTools\Sources\&lt;id&gt;\             projects created here
    ///     DevTools\recent-projects.txt        projects opened from anywhere else
    /// </summary>
    internal static class DevTools
    {
        internal static string Root => AppPaths.InAppDirectory("DevTools");

        internal static string Binaries => Path.Combine(Root, "Binaries");

        internal static string Sources => Path.Combine(Root, "Sources");

        internal static string RecentProjectsFile => Path.Combine(Root, "recent-projects.txt");

        internal static Twllink Twllink => new Twllink(Path.Combine(Binaries, Twllink.ExecutableName));
    }
}
