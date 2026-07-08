using System;
using System.Diagnostics;
using System.IO;

namespace ChangerAPI.Engine
{
    /// <summary>
    /// Shells out to the standalone LegacyDataConverter.exe (.NET Framework) companion tool to convert
    /// old BinaryFormatter-based files (skins, color presets) to their current formats in place.
    /// Ships next to the main Tweaker executable, not under Plugins\ - it's a service utility, not an
    /// optional plugin like the injector/overlay.
    /// </summary>
    public static class LegacyConverter
    {
        private const string ExecutableRelativePath = "LegacyDataConverter.exe";

        public static bool IsAvailable => File.Exists(GetConverterPath());

        public static bool TryConvert(string path)
        {
            var converterPath = GetConverterPath();
            if (!File.Exists(converterPath))
                return false;

            try
            {
                var startInfo = new ProcessStartInfo(converterPath, $"\"{path}\"")
                {
                    UseShellExecute = false,
                    CreateNoWindow = true,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true
                };

                using (var process = Process.Start(startInfo))
                {
                    process.WaitForExit();
                    return process.ExitCode == 0;
                }
            }
            catch (Exception)
            {
                return false;
            }
        }

        private static string GetConverterPath()
        {
            return Path.Combine(AppDomain.CurrentDomain.BaseDirectory, ExecutableRelativePath);
        }
    }
}
