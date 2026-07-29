using System;
using Avalonia;
using AudiosurfInterface.Bridge;
using TweakerUI.Core;

namespace TweakerUI
{
    internal sealed class Program
    {
        // Initialization code. Don't use any Avalonia, third-party APIs or any
        // SynchronizationContext-reliant code before AppMain is called: things aren't initialized
        // yet and stuff might break.
        [STAThread]
        public static void Main(string[] args)
        {
            // Before anything else on purpose: every Tweaker service starts later than this (the
            // bridge subprocess included - AudiosurfHandle is a lazy singleton first touched in
            // MainWindowViewModel's constructor), so a second instance bails out having spawned
            // nothing and having killed nobody else's asbridge.exe.
            if (!SingleInstanceGuard.TryAcquire())
            {
                // Silent focus handover is the whole point; the message box is the fallback for when
                // even that failed, not the normal second-launch experience.
                if (!SingleInstanceGuard.TryActivateRunningInstance())
                    SingleInstanceGuard.ShowAlreadyRunningMessage();
                return;
            }

            // Only now, holding the guard, is this process entitled to treat every other asbridge.exe
            // in the session as a stray worth killing - see BridgeProcessGuard.ExclusiveOwnership.
            BridgeProcessGuard.ExclusiveOwnership = true;

            try
            {
                BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
            }
            finally
            {
                SingleInstanceGuard.Release();
            }
        }

        // Avalonia configuration, don't remove; also used by visual designer.
        public static AppBuilder BuildAvaloniaApp()
            => AppBuilder.Configure<App>()
                .UsePlatformDetect()
#if DEBUG
                .WithDeveloperTools()
#endif
                .WithInterFont()
                .LogToTrace();
    }
}
