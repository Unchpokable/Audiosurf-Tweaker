using System;
using System.Diagnostics;
using System.IO;

namespace TweakerCore.Engine
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

        public static event Action<string, Exception> ConversionFailed;

        // Fired right before the external converter process starts - lets a UI layer show "converting
        // X" while it's actually happening, without duplicating SkinPackager.Decompile's own zip-magic-
        // bytes sniffing to guess in advance whether a given file needs conversion at all. Plain Action,
        // same as ConversionFailed - TweakerCore stays UI-agnostic, no reference to any status/notification
        // concept lives here.
        public static event Action<string> ConversionStarted;

        public static bool IsAvailable => File.Exists(GetConverterPath());

        public static bool TryConvert(string path)
        {
            var converterPath = GetConverterPath();
            if (!File.Exists(converterPath))
            {
                // Previously a silent `return false` - indistinguishable from "this file legitimately
                // isn't a legacy format" both to the caller and in the logs, which is exactly what made
                // the single-file-publish path bug above invisible (every legacy skin silently failed to
                // load, cache came out ~1KB, nothing recorded why).
                ConversionFailed?.Invoke(path, new FileNotFoundException(
                    $"LegacyDataConverter.exe not found at '{converterPath}' - legacy file left unconverted.", converterPath));
                return false;
            }

            try
            {
                ConversionStarted?.Invoke(path);

                var startInfo = new ProcessStartInfo(converterPath, $"\"{path}\"")
                {
                    UseShellExecute = false,
                    CreateNoWindow = true,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true
                };

                using (var process = Process.Start(startInfo))
                {
                    // LegacyDataConverter.exe writes every diagnostic (usage, "File not found", the
                    // full exception on a failed conversion) to stdout via Console.WriteLine, not
                    // stderr - reading StandardError here would always come back empty.
                    var stdout = process.StandardOutput.ReadToEnd();
                    process.WaitForExit();

                    if (process.ExitCode != 0)
                    {
                        ConversionFailed?.Invoke(path, new InvalidOperationException(
                            $"LegacyDataConverter.exe exited with code {process.ExitCode}: {stdout}"));
                        return false;
                    }

                    return true;
                }
            }
            catch (Exception ex)
            {
                ConversionFailed?.Invoke(path, ex);
                return false;
            }
        }

        // Path.GetDirectoryName(Environment.ProcessPath), NOT AppDomain.CurrentDomain.BaseDirectory:
        // the latter (same underlying value as AppContext.BaseDirectory) resolves to the
        // %TEMP%\.net\TweakerUI\<hash>\ self-extraction folder under TweakerUI's self-contained
        // single-file publish (Deploy.ps1, which needs IncludeNativeLibrariesForSelfExtract for
        // SkiaSharp/HarfBuzzSharp to run at all) - not the folder actually containing
        // LegacyDataConverter.exe. IsAvailable/TryConvert would silently look in the wrong place and
        // fail every time, converting nothing (see SkinChangerViewModel's own AppDirectory constant
        // for the same fix applied to the Skins folder lookup, root-caused the same way via logging).
        // Environment.ProcessPath is unaffected by self-extraction - it's TweakerUI.exe's own path.
        private static string GetConverterPath()
        {
            return Path.Combine(Path.GetDirectoryName(Environment.ProcessPath), ExecutableRelativePath);
        }
    }
}
