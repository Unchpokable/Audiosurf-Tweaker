using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Threading;
using TweakerCore.PluginInstall;
using TweakerUI.Core.Dialogs;
using TweakerUI.Views.Dialogs;

namespace TweakerUI.Core
{
    /// <summary>
    /// The host half of getting TweakerPlugin into the game and keeping it current
    /// (Docs/Internal/plugin-offline-mode.md §6.3-§6.6). TweakerCore's <see cref="PluginInstallation"/> does the
    /// file work and deliberately knows nothing about processes or dialogs; this is where the questions that
    /// need a user get asked - above all "Audiosurf is running, and these files are loaded into it right now".
    ///
    /// Where the plugin lives is not a setting: the DLL is either in engine\channels\ or it is not (Р-6). Every
    /// "is it installed" answer here is a fresh look at the disk, never a remembered flag.
    /// </summary>
    internal static class PluginService
    {
        internal const string PayloadFolderName = "PluginPayload";

        /// <summary>Raised whenever the answer to <see cref="IsInstalled"/> or <see cref="StatusText"/> may have changed.</summary>
        internal static event EventHandler Changed;

        /// <summary>
        /// The game's engine folder, derived from the one path the user configures. Null when the textures path
        /// is unset or no longer points at an Audiosurf install - which is a normal state, not an error: it is
        /// exactly what ConfigurationManager's "can't detect Audiosurf" fault path leaves behind.
        /// </summary>
        internal static string EngineDirectory => PluginInstallation.EngineFolderFromTexturesPath(SettingsProvider.GameTexturesPath);

        /// <summary>The game's root folder (the one holding Audiosurf.exe), or null.</summary>
        internal static string GameRootDirectory
        {
            get
            {
                var engine = EngineDirectory;
                return engine == null ? null : Directory.GetParent(engine)?.FullName;
            }
        }

        /// <summary>
        /// The bundle's mirror of the target layout (§6.1), next to TweakerUI.exe. Environment.ProcessPath, not
        /// the base directory: under single-file publish the latter is the self-extraction temp folder, which
        /// holds no PluginPayload at all (same reason ConfigurationManager resolves its config that way).
        /// </summary>
        internal static string PayloadDirectory =>
            Path.Combine(Path.GetDirectoryName(Environment.ProcessPath) ?? string.Empty, PayloadFolderName);

        internal static string InjectorPath =>
            Path.Combine(Path.GetDirectoryName(Environment.ProcessPath) ?? string.Empty, "InjectHelper.exe");

        internal static bool IsInstalled => PluginInstallation.IsInstalled(EngineDirectory);

        internal static string InstalledVersion => PluginInstallation.InstalledVersion(EngineDirectory);

        /// <summary>The DLL the game would load, i.e. what "Load now" injects, or null.</summary>
        internal static string InstalledPluginPath
        {
            get
            {
                var engine = EngineDirectory;
                return engine == null ? null : PluginInstallation.PluginPath(engine);
            }
        }

        internal static void NotifyChanged() => Changed?.Invoke(null, EventArgs.Empty);

        /// <summary>
        /// One line for the Settings tab and the connection status tooltip (§6.6). Recomputed on demand rather
        /// than cached: every input - the disk, the running game, the link - changes behind this class's back.
        /// </summary>
        internal static string StatusText
        {
            get
            {
                if (EngineDirectory == null)
                    return "Game folder unknown - set the textures path above";

                if (!IsInstalled)
                    return "Not installed";

                var version = InstalledVersion;
                var installed = version == null ? "Installed" : "Installed v" + version;

                if (!SettingsProvider.SyncOverlayWithTweaker)
                    return installed + "; not connected to Tweaker (overlay runs offline)";

                switch (OverlayHelper.LinkState)
                {
                    case PluginLinkState.Connected:
                        return $"Connected - v{OverlayHelper.ConnectedVersion} ({OverlayHelper.ConnectedLoadMode})";

                    case PluginLinkState.Waiting:
                        return "Loaded into the game, waiting for it to become ready";

                    case PluginLinkState.ProtocolMismatch:
                        return "The plugin loaded in the game speaks a different protocol - update it";

                    case PluginLinkState.NotLoadedInSession:
                        return installed + ", not loaded into the running game";

                    default:
                        return GameProcessService.IsRunning
                            ? installed + ", not loaded into the running game"
                            : installed + " (game not running)";
                }
            }
        }

        /// <summary>
        /// Installs, or updates an existing install. Returns whether the plugin is in the game folder afterwards.
        /// The running game is handled by <see cref="RunWithGameClosedAsync"/>, which is the whole of §6.4.
        /// </summary>
        internal static async Task<bool> InstallAsync()
        {
            var engine = EngineDirectory;
            if (engine == null)
            {
                ApplicationNotificationManager.Manager.ShowError("Plugin",
                    "The Audiosurf engine folder could not be found. Check the textures path in Settings, then try again.");
                return false;
            }

            if (!Directory.Exists(PayloadDirectory))
            {
                ApplicationNotificationManager.Manager.ShowError("Plugin",
                    $"'{PayloadDirectory}' is missing - this copy of Tweaker was built or unpacked without the plugin bundle.");
                return false;
            }

            if (!PluginInstallation.HasPlugin(PayloadDirectory))
            {
                ApplicationNotificationManager.Manager.ShowError("Plugin",
                    $"'{PayloadDirectory}' carries no {PluginInstallation.PluginRelativePath}. The bundle mirrors the game's own "
                    + $"layout, so the DLL has to sit in a {PluginInstallation.ChannelsFolderName}\\ subfolder of it. Nothing was installed.");
                return false;
            }

            // A first install writes into channels\ but replaces nothing the running game has open, so it does
            // not need the game closed; loading it into the session already running is the separate offer the
            // connection flow makes afterwards (§6.4).
            var isFirstInstall = !PluginInstallation.IsInstalled(engine);

            var report = await RunWithGameClosedAsync(
                isFirstInstall,
                "Tweaker needs to update the plugin files inside Audiosurf, but the game is running.",
                () => PluginInstallation.Install(engine, PayloadDirectory));

            NotifyChanged();

            if (report == null)
                return false;

            if (PluginInstallation.IsInstalled(engine))
            {
                // §6.4's second half: with the game already running, putting the files in place is only the
                // first step - that session has no plugin in it, and the user gets asked whether to load it now
                // or restart. Nothing else would ask: Registered fired before this install existed.
                OverlayHelper.Reevaluate();
                return true;
            }

            // Ran, reported no errors, and the plugin still is not there. Whatever the reason, saying nothing is
            // the one response that leaves the user watching a toggle flip back with no explanation.
            if (report.Success)
            {
                ApplicationNotificationManager.Manager.ShowError("Plugin",
                    $"Nothing went wrong, but {PluginInstallation.PluginRelativePath} did not end up in the game folder. "
                    + $"Check '{PayloadDirectory}' and the log.");
            }

            return false;
        }

        internal static async Task<bool> UninstallAsync()
        {
            var engine = EngineDirectory;
            if (engine == null)
                return false;

            if (!PluginInstallation.IsInstalled(engine))
            {
                NotifyChanged();
                return true;
            }

            var report = await RunWithGameClosedAsync(
                false,
                "Tweaker needs to remove the plugin from Audiosurf, but the game is running.",
                () => PluginInstallation.Uninstall(engine));

            NotifyChanged();

            if (report != null && !PluginInstallation.IsInstalled(engine))
            {
                OverlayHelper.ForgetLoadOffer();
                return true;
            }

            return false;
        }

        /// <summary>
        /// Startup check (§6.5): an installed plugin older than the bundle is brought up to date. Silent when
        /// there is nothing to do - which is every launch but the first one after an update.
        /// </summary>
        internal static async Task SyncOnStartupAsync()
        {
            var engine = EngineDirectory;
            if (engine == null || !PluginInstallation.IsInstalled(engine) || !Directory.Exists(PayloadDirectory))
                return;

            if (!await Task.Run(() => PluginInstallation.NeedsUpdate(engine, PayloadDirectory)))
                return;

            await InstallAsync();
        }

        /// <summary>
        /// §6.4 in one place: run <paramref name="action"/>, asking the user to deal with the running game first
        /// unless <paramref name="allowWhileRunning"/> says this particular action does not disturb it. Returns
        /// the report, or null when the user backed out and nothing ran.
        /// </summary>
        private static async Task<PluginInstallReport> RunWithGameClosedAsync(bool allowWhileRunning, string prompt, Func<PluginInstallReport> action)
        {
            var startGameAfterwards = false;

            if (!allowWhileRunning && GameProcessService.IsRunning)
            {
                var choice = await ApplicationNotificationManager.Manager.AskForChoice("Audiosurf is running",
                    prompt + " Close the game yourself, or let Tweaker close it.",
                    "I will close it", "Close it for me", "Cancel");

                switch (choice)
                {
                    case 0:
                        if (!await WaitForUserToCloseGameAsync())
                            return null;
                        // Not restarted afterwards: the user closed it, so whether it comes back is their call.
                        break;

                    case 1:
                        var confirmed = await ApplicationNotificationManager.Manager.AskForAction("Close Audiosurf",
                            "Audiosurf will be closed now. A run in progress will be lost. The game is started again once the plugin files are updated.");
                        if (!confirmed)
                            return null;

                        if (!await GameProcessService.CloseAsync())
                        {
                            ApplicationNotificationManager.Manager.ShowError("Plugin", "Audiosurf could not be closed - nothing was changed.");
                            return null;
                        }

                        startGameAfterwards = true;
                        break;

                    default:
                        return null;
                }
            }

            var report = await Task.Run(action);
            ReportToUser(report);

            if (startGameAfterwards)
                GameProcessService.Start(GameRootDirectory);

            return report;
        }

        /// <summary>
        /// Waits for the user to close the game, with a dialog whose only button aborts the wait. Returns false
        /// when they gave up instead - told apart from a successful wait by the game itself, not by the dialog:
        /// the dialog closes for both reasons.
        /// </summary>
        private static async Task<bool> WaitForUserToCloseGameAsync()
        {
            // Neither source is disposed: both tokens are handed to registrations that outlive this method by a
            // moment, and disposing one out from under a pending Task.Delay turns a clean cancellation into an
            // ObjectDisposedException nobody is there to observe. Neither holds a timer or a handle.
            var exited = new CancellationTokenSource();
            var abandoned = new CancellationTokenSource();

            _ = Task.Run(async () =>
            {
                if (await GameProcessService.WaitForExitAsync(abandoned.Token))
                    exited.Cancel();
            });

            await TweakerDialogWindow.ShowChoiceAsync(AppShell.MainWindow,
                "Waiting for Audiosurf to close. Close the game and Tweaker will carry on by itself.",
                "Waiting for Audiosurf",
                new[] { new TweakerDialogChoice("Cancel", isCancel: true) },
                exited.Token);

            abandoned.Cancel();
            return !GameProcessService.IsRunning;
        }

        /// <summary>
        /// Turns an install report into what the user needs to know. Files saved as ".old" are the part worth
        /// a dialog-free but explicit notification; everything else is a line in the log.
        /// </summary>
        private static void ReportToUser(PluginInstallReport report)
        {
            Logger.Log("PluginService",
                $"install: +{report.Added.Count} ~{report.Updated.Count} backed-up {report.UpdatedWithBackup.Count} " +
                $"removed {report.Removed.Count} released {report.Released.Count} skipped {report.SkippedDeleted.Count} " +
                $"errors {report.Errors.Count}");

            if (!report.Success)
            {
                var details = string.Join("\n", report.Errors.Select(FormatError));
                Post(() => ApplicationNotificationManager.Manager.ShowError("Plugin", $"Some plugin files could not be written:\n{details}"));
                return;
            }

            if (report.UpdatedWithBackup.Count > 0)
            {
                var saved = string.Join("\n", report.UpdatedWithBackup.Select(backup => $"{backup.RelativePath} -> {backup.BackupFileName}"));
                Post(() => ApplicationNotificationManager.Manager.ShowInformation("Plugin updated",
                    $"Files you had changed were updated. Your versions were kept next to them:\n{saved}"));
                return;
            }

            if (report.ChangedAnything)
                Post(() => ApplicationNotificationManager.Manager.ShowSuccess("Plugin", DescribeChange(report)));
        }

        private static string DescribeChange(PluginInstallReport report)
        {
            if (report.Removed.Contains(PluginInstallation.PluginRelativePath))
                return "The plugin was removed from Audiosurf. Your skyboxes, scripts and configs were left alone.";

            var parts = new List<string>();
            if (report.Added.Count > 0)
                parts.Add(report.Added.Count + " added");
            if (report.Updated.Count > 0)
                parts.Add(report.Updated.Count + " updated");
            if (report.Removed.Count > 0)
                parts.Add(report.Removed.Count + " removed");

            return parts.Count == 0 ? "Plugin files are up to date." : "Plugin files: " + string.Join(", ", parts) + ".";
        }

        private static string FormatError(PluginInstallError error)
        {
            return error.RelativePath == null ? error.Message : error.RelativePath + ": " + error.Message;
        }

        private static void Post(Action action) => Dispatcher.UIThread.Post(action);
    }
}
