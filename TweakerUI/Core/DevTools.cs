using System.IO;
using TweakerCore.Scripting;

namespace TweakerUI.Core
{
    /// <summary>
    /// &lt;TweakerUI&gt;\DevTools\ - the script developer's side of the app (Docs/Internal/script-manager.md
    /// §6, §8.1). Next to the exe like the rest of the app's data (AppPaths), not in the game:
    ///
    ///     DevTools\Binaries\twllink.exe       the linker; Native AOT in a release, a plain build in a dev one
    ///     DevTools\CgrPy\                     the game's reverse-engineering tools, cgrpy (dev-bundle-roadmap.md §3.2)
    ///     DevTools\Docs\                      the public documentation; copied into a project set up for agents
    ///     DevTools\AgentKit\                  what Script Dev puts into a project for AI agents (Tools/AgentKit)
    ///     DevTools\Sources\&lt;id&gt;\             projects created here
    ///     DevTools\recent-projects.txt        projects opened from anywhere else
    /// </summary>
    internal static class DevTools
    {
        internal static string Root => AppPaths.InAppDirectory("DevTools");

        internal static string Binaries => Path.Combine(Root, "Binaries");

        internal static string CgrPy => Path.Combine(Root, "CgrPy");

        /// <summary>What an editor opened from Script Dev gets at the front of its PATH, in this order.</summary>
        internal static string[] ToolFolders => new[] { Binaries, CgrPy };

        internal static string Docs => Path.Combine(Root, "Docs");

        internal static string AgentKit => Path.Combine(Root, "AgentKit");

        internal static string Sources => Path.Combine(Root, "Sources");

        internal static string RecentProjectsFile => Path.Combine(Root, "recent-projects.txt");

        internal static Twllink Twllink => new Twllink(Path.Combine(Binaries, Twllink.ExecutableName));
    }
}
