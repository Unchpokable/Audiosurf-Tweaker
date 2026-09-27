using System;
using System.IO;

namespace TweakerUI.Core
{
    // Where the app's own data lives (skins, palettes) - next to TweakerUI.exe.
    //
    // Path.GetDirectoryName(Environment.ProcessPath), NOT AppContext.BaseDirectory and NOT a bare
    // relative path - both of the latter broke under Deploy.ps1's self-contained single-file publish,
    // confirmed by logging it and reading it back:
    //   SkinsRootPath='C:\Users\...\AppData\Local\Temp\.net\TweakerUI\<hash>\Skins' (exists=False)
    // A relative path resolves against the process's current working directory, not the exe's own
    // folder - those only coincide by accident (e.g. double-clicking from the same folder), which is
    // what made the app's behavior depend on launch method/CWD instead of where the exe actually lives
    // (found once for the skins folder, and again for the palettes file, which still used a relative
    // path - KnownBugs #7). AppContext.BaseDirectory looks like the fix (it's the commonly-cited
    // single-file-safe alternative to Assembly.Location) but is ALSO wrong here: Deploy.ps1 publishes
    // with IncludeNativeLibrariesForSelfExtract/IncludeAllContentForSelfExtract (needed so
    // SkiaSharp/HarfBuzzSharp's native assets can run at all from inside a single-file bundle), and
    // once content self-extraction is enabled, AppContext.BaseDirectory reports the
    // %TEMP%\.net\TweakerUI\<hash>\ extraction folder, not the folder containing TweakerUI.exe.
    // Environment.ProcessPath is unaffected by self-extraction - it's the actual apphost .exe's own
    // path in every publish mode (ConfigurationManager.ExePath relies on the same thing).
    internal static class AppPaths
    {
        public static readonly string AppDirectory = Path.GetDirectoryName(Environment.ProcessPath);

        public static string InAppDirectory(string relative) => Path.Combine(AppDirectory, relative);
    }
}
