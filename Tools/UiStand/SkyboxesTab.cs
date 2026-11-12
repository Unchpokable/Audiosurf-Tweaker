using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Animation;
using Avalonia.Controls;
using Avalonia.Controls.Presenters;
using Avalonia.Headless;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Styling;
using Avalonia.Threading;
using Avalonia.VisualTree;
using TweakerCore.PluginInstall;
using TweakerCore.Skyboxes;
using TweakerUI.Core;
using TweakerUI.Models;
using TweakerUI.ViewModels;

// Real view models, packager and watchers over a temporary game. Only the IPC transport is fake.
static class SkyboxesTab
{
    private static int _checks;
    private static int _failures;

    private static void Check(bool condition, string description)
    {
        _checks++;
        if (!condition)
        {
            _failures++;
            Console.WriteLine("FAIL " + description);
        }
    }

    public static int Run(string outDir)
    {
        Directory.CreateDirectory(outDir);
        var root = Path.Combine(Path.GetTempPath(), "UiStand-skyboxes-" + Guid.NewGuid().ToString("N"));
        var engine = Path.Combine(root, "Audiosurf", "engine");
        var devTools = Path.Combine(Path.GetDirectoryName(Environment.ProcessPath)!, "DevTools");
        var recents = Path.Combine(devTools, "recent-sky-projects.txt");
        var previousRecents = File.Exists(recents) ? File.ReadAllBytes(recents) : null;
        var created = new List<string>();
        var live = false;
        var sent = new List<string>();
        SkyboxesViewModel vm = null;

        try
        {
            Directory.CreateDirectory(Path.Combine(engine, "textures"));
            Directory.CreateDirectory(Path.Combine(engine, "channels"));
            Directory.CreateDirectory(SkyPaths.SkyboxesPath(engine));
            File.WriteAllText(Path.Combine(engine, "QuestViewer.exe"), "fixture, never executed");
            File.WriteAllText(Path.Combine(engine, "channels", "TweakerPlugin.dll"), "fixture");
            File.WriteAllText(Path.Combine(SkyPaths.SkyboxesPath(engine), "Bundled.sky"), "fixture");
            File.WriteAllText(Path.Combine(SkyPaths.SkyboxesPath(engine), "Yours.sky"), "fixture");
            File.WriteAllText(Path.Combine(SkyPaths.SkyboxesPath(engine), "stand.hlsl"), "fixture");
            var manifest = new PluginDeployManifest();
            manifest.Files[SkyPaths.SkyboxesRelativeFolder + "Bundled.sky"] = new PluginDeployFile { Sha256 = "fixture" };
            manifest.Save(engine);
            SetTextures(Path.Combine(engine, "textures"));

            SkyModuleConfig.Write(engine, true, new SkySelection(SkyKind.Package, "Yours.sky"));
            var module = SkyPaths.ModuleConfigPath(engine);
            var config = File.ReadAllText(module).Replace("{", "{ // keep this comment\n", StringComparison.Ordinal);
            File.WriteAllText(module, config);
            SkyboxOverlayBridge.UseTransport(line => { sent.Add(line); return true; }, () => live);
            typeof(SkyboxOverlayBridge).GetMethod("Initialize", BindingFlags.Static | BindingFlags.NonPublic)!.Invoke(null, null);

            vm = new SkyboxesViewModel();
            vm.SetActive(true);
            Await(vm.RefreshAsync());
            Check(vm.Skies.Count == 3, "offline catalog includes packages and loose shader");
            Check(vm.Skies.Single(row => row.Entry.Id == "Bundled.sky").CanDelete == false, "bundled sky is protected");
            Check(vm.Skies.Single(row => row.Entry.Id == "Yours.sky").CanDelete, "own sky can be deleted");
            Shoot(outDir, "game-offline", vm);
            CheckPageHeaders(vm);

            var plugin = Path.Combine(engine, "channels", "TweakerPlugin.dll");
            File.Move(plugin, plugin + ".off");
            Await(vm.RefreshAsync());
            Check(vm.Notice?.Contains("not installed") == true, "missing plugin banner");
            Shoot(outDir, "game-no-plugin", vm);
            File.Move(plugin + ".off", plugin);
            Await(vm.RefreshAsync());

            if (!vm.GameRunning)
            {
                vm.Enabled = false;
                Check(!SkyModuleConfig.Load(engine).Enabled && File.ReadAllText(module).Contains("keep this comment"), "offline toggle preserves comments");
                vm.SelectedSky = vm.Skies.Single(row => row.Entry.Id == "stand.hlsl");
                Check(SkyModuleConfig.Load(engine).Sky.Matches(SkyKind.ShaderFile, "stand.hlsl"), "offline selection writes config");
            }
            else
            {
                Console.WriteLine("note: real Audiosurf is running; offline-write checks were not executed");
            }

            File.WriteAllText(module, "{ broken");
            Await(vm.RefreshAsync());
            Check(!vm.CanControl && vm.Notice.Contains("module.json"), "broken config locks controls");
            Shoot(outDir, "game-broken-config", vm);
            File.WriteAllText(module, config);
            Await(vm.RefreshAsync());

            vm.Page = 1;
            vm.StartCreateCommand.Execute(null);
            vm.ProjectName = "bad/name";
            Await(vm.CreateCommand.ExecuteAsync(null));
            Check(vm.IsCreating && vm.NameProblem != null, "invalid project file name is refused");
            Shoot(outDir, "project-new-invalid", vm);

            var prefix = "Stand Sky " + Guid.NewGuid().ToString("N")[..7];
            foreach (var template in vm.Templates)
            {
                vm.StartCreateCommand.Execute(null);
                vm.ProjectName = prefix + " " + template.Title;
                vm.ProjectAuthor = "Sky author";
                vm.Template = template;
                Shoot(outDir, "project-new-" + template.Key, vm);
                Await(vm.CreateCommand.ExecuteAsync(null));
                var row = vm.SelectedProject;
                Check(row != null && row.Project.Name == vm.ProjectName, "create and select " + template.Title);
                created.Add(row.Root);
                Check(SkyPackager.Check(row.Root).All(problem => !problem.IsError), template.Title + " passes package validation");
                Check(File.Exists(Path.Combine(row.Root, ".tweaker", "include", "sky_common.hlsli")), template.Title + " has editor headers");
            }

            var project = vm.SelectedProject;
            var projectRoot = project.Root;
            var export = Path.Combine(root, "export.sky");
            Await(vm.ExportProjectAsync(project, export));
            Check(File.Exists(export) && project.Result.StartsWith("Packed to"), "export writes a real archive");
            using (var zip = ZipFile.OpenRead(export))
                Check(zip.GetEntry("Config.json") != null && zip.Entries.All(entry => !entry.FullName.StartsWith(".") && entry.Name != "shadertoolsconfig.json"), "archive excludes editor support");

            vm.StartEditCommand.Execute(null);
            vm.ProjectName += " renamed";
            Check(vm.RenameWarning != null, "rename warning precedes saving");
            vm.ProjectVersion = "2";
            Shoot(outDir, "project-edit", vm);
            vm.SaveEditCommand.Execute(null);
            project = vm.SelectedProject;
            Check(project.Project.Version == 2 && project.Title.EndsWith("renamed"), "manifest edits name and version");

            Await(vm.InstallCommand.ExecuteAsync(null));
            var installed = Path.Combine(SkyPaths.SkyboxesPath(engine), project.PackageFileName);
            Check(File.Exists(installed) && project.Result.StartsWith("Installed at"), "install writes package");
            Shoot(outDir, "project-installed", vm);

            var shader = Path.Combine(projectRoot, "shaders", "sky.hlsl");
            var bytes = File.ReadAllBytes(installed);
            File.Move(shader, shader + ".saved");
            Await(vm.InstallCommand.ExecuteAsync(null));
            Check(project.Problems.Any(problem => problem.IsError) && bytes.SequenceEqual(File.ReadAllBytes(installed)), "invalid source leaves installed package untouched");
            Shoot(outDir, "project-problems", vm);
            File.Move(shader + ".saved", shader);

            File.Delete(installed);
            Directory.CreateDirectory(installed);
            File.WriteAllText(Path.Combine(installed, "keep.txt"), "keep");
            Await(vm.InstallCommand.ExecuteAsync(null));
            Check(project.Result.Contains("folder") && File.Exists(Path.Combine(installed, "keep.txt")), "directory collision is refused without altering it");
            Shoot(outDir, "project-collision", vm);
            Directory.Delete(installed, true);

            live = true;
            SkyboxOverlayBridge.RefreshLink();
            Check(sent.Last() == "SKY_RESCAN", "link requests a full catalog after handshake continuation");
            var beforeLive = File.ReadAllText(module);
            FeedCatalog(project.PackageFileName);
            typeof(SkyboxesViewModel).Assembly.GetType("TweakerUI.Core.OverlayHelper")!
                .GetMethod("OnOverlayMessageReceived", BindingFlags.Static | BindingFlags.NonPublic)!
                .Invoke(null, new object[] { null, "SKY_STATE true packed skyboxes%2Fcloudy_01.png ready" });
            Dispatcher.UIThread.RunJobs();
            Check(SkyboxOverlayBridge.State?.Sky.Kind == SkyKind.Packed, "OverlayHelper routes SKY messages to the UI bridge");
            Check(SkyboxOverlayBridge.IsSynced && vm.Skies.Count == 4, "live catalog includes built-ins and replaces disk list");
            Check(vm.Skies.Single(row => row.Entry.Kind == SkyKind.Packed).CanDelete == false, "built-in sky protected");
            vm.Page = 0;
            Shoot(outDir, "game-live", vm);

            ClickSky(vm, SkyKind.Program);
            Check(sent.Last() == "SKY_SELECT program gradient" && SkyboxOverlayBridge.PendingSelection?.Id == "gradient",
                "a real pointer click selects the sky and reaches the transport");
            SkyboxOverlayBridge.Receive("SKY_STATE true packed skyboxes%2Fcloudy_01.png ready");

            vm.Enabled = false;
            Check(sent.Last() == "SKY_ENABLE false" && SkyboxOverlayBridge.PendingEnabled == false, "live toggle waits for confirmation");
            Check(File.ReadAllText(module) == beforeLive, "live toggle never writes module.json");
            Shoot(outDir, "game-pending", vm);
            SkyboxOverlayBridge.Receive("SKY_STATE true packed skyboxes%2Fcloudy_01.png ready");
            Check(vm.Enabled && !SkyboxOverlayBridge.IsPending, "authoritative response rolls rejected toggle back");

            vm.SelectedSky = vm.Skies.Single(row => row.Entry.Kind == SkyKind.Program);
            Check(sent.Last() == "SKY_SELECT program gradient" && SkyboxOverlayBridge.PendingSelection?.Id == "gradient", "live selection sent");
            SkyboxOverlayBridge.Receive("SKY_STATE true program gradient compiling");
            Check(!SkyboxOverlayBridge.IsPending && vm.ActiveText.Contains("compiling"), "compiling response confirms selection");
            SkyboxOverlayBridge.Receive("SKY_STATE true program gradient failed sky%3A2%20broken");
            Check(vm.LoadProblem.Contains("sky:2 broken"), "failure reason visible");
            Shoot(outDir, "game-failed", vm);
            SkyboxOverlayBridge.Receive("SKY_STATE true package Gone.sky missing");
            Check(vm.SelectedSky == null && vm.ActiveText.Contains("Gone.sky"), "missing selection retained without inventing a row");
            Shoot(outDir, "game-missing", vm);

            SkyboxOverlayBridge.Receive("SKY_ENTRY package Partial.sky Partial");
            SkyboxOverlayBridge.Receive("SKY_SYNCED 2");
            Check(!SkyboxOverlayBridge.IsSynced && SkyboxOverlayBridge.Catalog.Count == 4, "truncated snapshot does not replace catalog");
            SkyboxOverlayBridge.Receive("SKY_SYNCED 0");
            Check(SkyboxOverlayBridge.IsSynced && SkyboxOverlayBridge.Catalog.Count == 0, "empty snapshot replaces catalog");
            FeedCatalog(project.PackageFileName);

            vm.Page = 1;
            sent.Clear();
            Await(vm.InstallAndUseCommand.ExecuteAsync(null));
            Check(sent.SequenceEqual(new[] { "SKY_RESCAN", SkyWire.Select(new SkySelection(SkyKind.Package, project.PackageFileName)) }), "install and use rescans then selects");
            Check(File.ReadAllText(module) == beforeLive && File.Exists(installed), "live install writes package only");
            SkyboxOverlayBridge.Receive("SKY_STATE true package " + Uri.EscapeDataString(project.PackageFileName) + " compiling");
            Check(project.Result.Contains("compiling"), "project status waits for compilation");
            SkyboxOverlayBridge.Receive("SKY_STATE true package " + Uri.EscapeDataString(project.PackageFileName) + " ready");
            Check(project.Result.Contains("live from the game"), "ready completes project status");
            SkyboxOverlayBridge.Receive("SKY_STATE true package " + Uri.EscapeDataString(project.PackageFileName) + " failed shader%20error");
            Check(vm.ProjectGameStatus.Contains("failed") && vm.ProjectGameStatus.Contains("shader error"), "project shows failures arriving after selection acknowledgement");
            Shoot(outDir, "project-live-failed", vm);
            SkyboxOverlayBridge.Receive("SKY_STATE true package " + Uri.EscapeDataString(project.PackageFileName) + " ready");
            Shoot(outDir, "project-live", vm);

            // Save while hidden. The project watcher remains active; one save must install and request one reload.
            vm.InstallOnSave = true;
            vm.SetActive(false);
            sent.Clear();
            File.AppendAllText(shader, "\n// a save in the editor\n");
            Pump(() => sent.Contains("SKY_RESCAN") && sent.Any(line => line.StartsWith("SKY_SELECT")), 5000);
            Idle(750);
            Check(sent.Count(line => line.StartsWith("SKY_SELECT")) == 1, "hidden install-on-save reloads active sky once");
            sent.Clear();
            File.AppendAllText(Path.Combine(projectRoot, ".tweaker", "include", "sky_common.hlsli"), "\n// editor-only change\n");
            Idle(500);
            Check(sent.Count == 0, "editor support is not a project save");
            vm.InstallOnSave = false;
            File.AppendAllText(shader, "\n// watcher is off\n");
            Idle(500);
            Check(sent.Count == 0, "disabled install-on-save does no work");
            vm.SetActive(true);
            Await(vm.RefreshAsync());

            sent.Clear();
            Await(vm.RefreshCommand.ExecuteAsync(null));
            Check(sent.Count(line => line == "SKY_RESCAN") == 1, "refresh asks plugin once");
            FeedCatalog(project.PackageFileName);

            SkyboxOverlayBridge.ConfirmTimeout = TimeSpan.FromMilliseconds(180);
            var timeouts = 0;
            EventHandler timedOut = (_, _) => timeouts++;
            SkyboxOverlayBridge.RequestTimedOut += timedOut;
            SkyboxOverlayBridge.RequestEnable(false);
            Idle(110);
            SkyboxOverlayBridge.RequestEnable(true);
            Idle(110);
            Check(SkyboxOverlayBridge.PendingEnabled == true && timeouts == 0, "new click replaces earlier timeout");
            Idle(110);
            Check(!SkyboxOverlayBridge.IsPending && timeouts == 1, "latest unanswered request expires exactly once");
            SkyboxOverlayBridge.RequestSelect(new SkySelection(SkyKind.Program, "gradient"));
            SkyboxOverlayBridge.Receive("SKY_STATE true program gradient failed test");
            Idle(210);
            Check(timeouts == 1 && !SkyboxOverlayBridge.IsPending, "failure response cancels timeout");
            SkyboxOverlayBridge.RequestTimedOut -= timedOut;

            // A minimized fullscreen game does not run its overlay frame. The host's timeout must
            // neither send an undo command nor reject the response when the game finally resumes.
            SkyboxOverlayBridge.Receive("SKY_STATE true packed skyboxes%2Fcloudy_01.png ready");
            sent.Clear();
            ClickSky(vm, SkyKind.Program);
            Idle(250);
            Check(!SkyboxOverlayBridge.IsPending && vm.SelectedSky?.Entry.Kind == SkyKind.Packed,
                "without game frames the selection times out to the last confirmed state");
            Check(sent.SequenceEqual(new[] { "SKY_SELECT program gradient" }),
                "the host does not send an undo selection when game frames stop");
            SkyboxOverlayBridge.Receive("SKY_STATE true program gradient ready");
            Check(vm.SelectedSky?.Entry.Kind == SkyKind.Program && sent.Count == 1,
                "the resumed game's late selection acknowledgement is applied without another command");

            SkyboxOverlayBridge.RequestEnable(false);
            live = false;
            vm.GameRunning = true;
            SkyboxOverlayBridge.RefreshLink();
            vm.Page = 0;
            vm.GameRunning = true;
            Await(vm.RefreshAsync());
            Check(!SkyboxOverlayBridge.IsPending && SkyboxOverlayBridge.State == null && SkyboxOverlayBridge.Catalog.Count == 0, "disconnect clears cache and requests");
            Check(!vm.CanControl && vm.Notice.Contains("without a connection"), "running disconnected locks controls");
            vm.Enabled = !vm.Enabled;
            Check(File.ReadAllText(module) == beforeLive, "locked control cannot write config");
            Shoot(outDir, "game-disconnected", vm);

            // Send failure and synchronous response must not leave a false pending state.
            live = true;
            SkyboxOverlayBridge.UseTransport(_ => false, () => live);
            Check(!SkyboxOverlayBridge.RequestEnable(true) && !SkyboxOverlayBridge.IsPending, "send failure leaves no pending state");
            SkyboxOverlayBridge.UseTransport(_ => { SkyboxOverlayBridge.Receive("SKY_STATE true program gradient ready"); return true; }, () => live);
            Check(SkyboxOverlayBridge.RequestEnable(true) && !SkyboxOverlayBridge.IsPending, "synchronous confirmation is not overwritten");

            var broken = Path.Combine(root, "Broken project");
            Directory.CreateDirectory(broken);
            File.WriteAllText(Path.Combine(broken, "Config.json"), "{ broken");
            vm.OpenProject(broken);
            Check(vm.SelectedProject.Problem != null, "broken project listed with reason");
            vm.Page = 1;
            Shoot(outDir, "project-broken", vm);
            Directory.Delete(broken, true);
            vm.RefreshProjectsCommand.Execute(null);
            Check(vm.SelectedProject.IsMissing, "remembered missing project remains visible");
            Shoot(outDir, "project-missing", vm);
            vm.ForgetProjectCommand.Execute(null);
            Check(vm.Projects.All(row => row.Root != broken), "forget removes missing external project");

            live = false;
            SkyboxOverlayBridge.RefreshLink();
            SetTextures(null);
            Await(vm.RefreshAsync());
            vm.Page = 0;
            Check(vm.Notice.Contains("unknown") && !vm.CanControl, "unknown game folder banner");
            Shoot(outDir, "game-no-folder", vm);

            var disposed = DisposedViewModel();
            for (var i = 0; i < 3; i++)
            {
                Dispatcher.UIThread.RunJobs();
                GC.Collect();
                GC.WaitForPendingFinalizers();
            }
            Check(!disposed.IsAlive, "disposed view model is not rooted by bridge subscriptions");
        }
        finally
        {
            vm?.Dispose();
            live = false;
            SkyboxOverlayBridge.RefreshLink();
            SkyboxOverlayBridge.ConfirmTimeout = TimeSpan.FromSeconds(5);
            if (previousRecents != null)
                File.WriteAllBytes(recents, previousRecents);
            else if (File.Exists(recents))
                File.Delete(recents);

            foreach (var path in created)
            {
                if (Directory.Exists(path) && Path.GetFullPath(path).StartsWith(Path.GetFullPath(Path.Combine(devTools, "SkySources")) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
                    Directory.Delete(path, true);
            }
            if (Directory.Exists(root))
                Directory.Delete(root, true);
        }

        Console.WriteLine($"skyboxes: {_checks} checks, {_failures} failed; snapshots in {outDir}");
        return _failures == 0 ? 0 : 1;
    }

    private static void FeedCatalog(string projectFile)
    {
        SkyboxOverlayBridge.Receive("SKY_ENTRY program gradient Gradient");
        SkyboxOverlayBridge.Receive("SKY_ENTRY packed skyboxes%2Fcloudy_01.png Cloudy 256");
        SkyboxOverlayBridge.Receive("SKY_ENTRY package Bundled.sky Bundled");
        SkyboxOverlayBridge.Receive("SKY_ENTRY package " + Uri.EscapeDataString(projectFile) + " My%20sky");
        SkyboxOverlayBridge.Receive("SKY_SYNCED 4");
    }

    private static void ClickSky(SkyboxesViewModel vm, SkyKind kind)
    {
        vm.Page = 0;
        var window = new Window { Width = 738, Height = 665, Content = new TweakerUI.Views.SkyboxesView { DataContext = vm } };
        window.Show();
        Dispatcher.UIThread.RunJobs();
        var item = window.GetVisualDescendants().OfType<ListBoxItem>()
            .Single(control => control.DataContext is SkyRowViewModel row && row.Entry.Kind == kind);
        var point = item.TranslatePoint(new Point(item.Bounds.Width / 2, item.Bounds.Height / 2), window)!.Value;
        window.MouseMove(point);
        window.MouseDown(point, MouseButton.Left);
        window.MouseUp(point, MouseButton.Left);
        Dispatcher.UIThread.RunJobs();
        window.Close();
    }

    private static void CheckPageHeaders(SkyboxesViewModel vm)
    {
        var window = new Window { Width = 738, Height = 665, Content = new TweakerUI.Views.SkyboxesView { DataContext = vm } };
        window.Show();
        Dispatcher.UIThread.RunJobs();
        var pages = window.GetVisualDescendants().OfType<TabItem>().ToArray();
        var game = pages.Single(item => (string)item.Header == "Game");
        var projects = pages.Single(item => (string)item.Header == "Projects");

        foreach (var theme in new[] { ThemeVariant.Light, ThemeVariant.Dark })
        {
            Application.Current!.RequestedThemeVariant = theme;
            Dispatcher.UIThread.RunJobs();
            var border = game.GetVisualDescendants().OfType<Border>().Single(item => item.Name == "PART_Border");
            var presenter = game.GetVisualDescendants().OfType<ContentPresenter>().Single(item => item.Name == "PART_HeaderPresenter");
            Check(border.Transitions?.OfType<CrossfadeBrushTransition>().Any() == true
                && presenter.Transitions?.OfType<CrossfadeBrushTransition>().Any() == true, theme + " headers animate their painted background and text");
            Check(((ISolidColorBrush)border.GetBaseValue(Border.BackgroundProperty).Value).Color
                == ((ISolidColorBrush)game.FindResource(theme, "AccentPrimaryBrush")).Color, theme + " selected header follows app accent");
            Check(((ISolidColorBrush)presenter.GetBaseValue(ContentPresenter.ForegroundProperty).Value).Color
                == ((ISolidColorBrush)game.FindResource(theme, "TextOnAccentBrush")).Color, theme + " selected header text follows app theme");
        }

        var point = projects.TranslatePoint(new Point(projects.Bounds.Width / 2, projects.Bounds.Height / 2), window)!.Value;
        window.MouseMove(point);
        window.MouseDown(point, MouseButton.Left);
        window.MouseUp(point, MouseButton.Left);
        Dispatcher.UIThread.RunJobs();
        Check(vm.Page == 1 && projects.IsSelected, "clicking Projects changes the bound page");
        vm.Page = 0;
        Dispatcher.UIThread.RunJobs();
        Check(game.IsSelected && !projects.IsSelected, "view model can select the Game header");
        window.Close();
    }

    [System.Runtime.CompilerServices.MethodImpl(System.Runtime.CompilerServices.MethodImplOptions.NoInlining)]
    private static WeakReference DisposedViewModel()
    {
        var vm = new SkyboxesViewModel();
        vm.Dispose();
        return new WeakReference(vm);
    }

    private static void SetTextures(string path) => typeof(SkyboxesViewModel).Assembly.GetType("TweakerUI.Core.SettingsProvider")!
        .GetProperty("GameTexturesPath", BindingFlags.Static | BindingFlags.NonPublic)!.SetValue(null, path);

    private static void Await(Task task)
    {
        Pump(() => task.IsCompleted, 15000);
        task.GetAwaiter().GetResult();
    }

    private static void Pump(Func<bool> completed, int milliseconds)
    {
        var clock = Stopwatch.StartNew();
        while (!completed() && clock.ElapsedMilliseconds < milliseconds)
        {
            Dispatcher.UIThread.RunJobs();
            System.Threading.Thread.Sleep(10);
        }
        Dispatcher.UIThread.RunJobs();
        if (!completed())
            throw new TimeoutException("UiStand skyboxes operation did not finish");
    }

    private static void Idle(int milliseconds)
    {
        var clock = Stopwatch.StartNew();
        while (clock.ElapsedMilliseconds < milliseconds)
        {
            Dispatcher.UIThread.RunJobs();
            System.Threading.Thread.Sleep(10);
        }
    }

    private static void Shoot(string directory, string name, SkyboxesViewModel vm)
    {
        foreach (var theme in new[] { ThemeVariant.Light, ThemeVariant.Dark })
        {
            Application.Current!.RequestedThemeVariant = theme;
            foreach (var width in new[] { 738, 1658 })
            {
                var window = new Window { Width = width, Height = 665, Content = new TweakerUI.Views.SkyboxesView { DataContext = vm } };
                window.Show();
                for (var i = 0; i < 5; i++)
                    Dispatcher.UIThread.RunJobs();
                foreach (var element in window.GetVisualDescendants().OfType<Animatable>())
                    element.Transitions = null;
                Dispatcher.UIThread.RunJobs();
                using (var frame = window.CaptureRenderedFrame())
                    frame!.Save(Path.Combine(directory, $"{theme}-{name}-{width}.png"));
                window.Close();
            }
        }
    }
}
