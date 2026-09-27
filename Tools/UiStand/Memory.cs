using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Threading;
using Avalonia.Controls;
using Avalonia.Headless;
using Avalonia.Threading;
using TweakerUI.ViewModels;

// Memory of the Skin Changer tab with the REAL SkinChangerViewModel (and its view, so bindings hold what they
// hold in the app): after loading the list, after every skin selection, around switching Fast Previews.
// Needs skins in Skins\ next to UiStand.exe - the view model resolves it from Environment.ProcessPath.
//
// Each line: managed = GC.GetTotalMemory (live + not yet collected), private = process private bytes
// (managed heap + native Skia pixels + everything else). "+gc" lines are after a forced full collection -
// the difference between a line and its "+gc" twin is garbage the GC simply hadn't got to.
static class Memory
{
    static readonly Type Settings = typeof(SkinChangerViewModel).Assembly.GetType("TweakerUI.Core.SettingsProvider");

    static void SetSetting(string name, object value) =>
        Settings.GetProperty(name, BindingFlags.Static | BindingFlags.NonPublic).SetValue(null, value);

    static void Pump(Func<bool> done, int timeoutMs = 120000)
    {
        var sw = Stopwatch.StartNew();
        while (!done())
        {
            Dispatcher.UIThread.RunJobs();
            if (sw.ElapsedMilliseconds > timeoutMs)
                throw new TimeoutException();
            Thread.Sleep(5);
        }
        for (int i = 0; i < 5; i++)
            Dispatcher.UIThread.RunJobs();
    }

    static void Idle(int ms)
    {
        var sw = Stopwatch.StartNew();
        Pump(() => sw.ElapsedMilliseconds >= ms);
    }

    static void Report(string label, bool alsoAfterGc = false)
    {
        Line(label);
        if (!alsoAfterGc)
            return;
        GC.Collect(GC.MaxGeneration, GCCollectionMode.Forced, true, true);
        GC.WaitForPendingFinalizers();
        GC.Collect(GC.MaxGeneration, GCCollectionMode.Forced, true, true);
        Line(label + " +gc");
        // UISTAND_PAUSE=<part of a label>: hold here so a heap dump can be taken (dotnet-gcdump collect -p <pid>).
        var pause = Environment.GetEnvironmentVariable("UISTAND_PAUSE");
        if (!string.IsNullOrEmpty(pause) && label.Contains(pause))
        {
            Console.WriteLine($"PAUSE pid {Environment.ProcessId}");
            Thread.Sleep(40000);
        }
    }

    static void Line(string label)
    {
        using var p = Process.GetCurrentProcess();
        var info = GC.GetGCMemoryInfo();
        Console.WriteLine($"{label,-34} managed {Mb(GC.GetTotalMemory(false)),7}  committed {Mb(info.TotalCommittedBytes),7}  private {Mb(p.PrivateMemorySize64),7}  ws {Mb(p.WorkingSet64),7}");
    }

    static string Mb(long bytes) => $"{bytes / 1048576.0:F1}";

    static long HeldPixels(SkinChangerViewModel vm) =>
        vm.Skins.Where(c => c.Screenshots != null).SelectMany(c => c.Screenshots)
            .Concat(vm.SelectedItemScreenshots)
            .Select(s => s.Image).Distinct()
            .Sum(b => (long)b.PixelSize.Width * b.PixelSize.Height * 4);

    // Renders a frame: a bitmap disposed while an Image still shows it throws here (pitfalls §6).
    static void Frame(Window window, string after)
    {
        try
        {
            window.CaptureRenderedFrame()?.Dispose();
        }
        catch (Exception ex)
        {
            Console.WriteLine($"  !! frame after {after}: {ex.GetType().Name}: {ex.Message}");
        }
    }

    static bool IsDisposed(Avalonia.Media.Imaging.Bitmap bitmap)
    {
        try
        {
            _ = bitmap.PixelSize; // goes through the ref-counted impl, which throws once disposed
            return false;
        }
        catch (ObjectDisposedException)
        {
            return true;
        }
    }

    // Ownership of the selection's screenshots: rapid re-selection must leave exactly the last card's set in
    // the panel, and an open big-picture window must hold off disposal of its bitmap until it closes.
    static void Checks(SkinChangerViewModel vm, Window window)
    {
        var cards = vm.Skins.Where(c => c.Screenshots == null && TweakerCore.Engine.SkinPackager.ReadPreviews(c.PathToOrigin, bitmap => 0).Count > 1).Take(2).ToList();
        if (cards.Count < 2)
            return;

        var a = cards[0];
        var b = cards[1];
        vm.SelectedItem = a;
        vm.SelectedItem = b;
        vm.SelectedItem = a;
        Pump(() => !vm.IsLoadingScreenshots);
        Idle(500); // let the two losing reads finish too
        var shown = vm.SelectedItemScreenshots.ToList();
        var expected = TweakerCore.Engine.SkinPackager.ReadPreviews(a.PathToOrigin, bitmap => 0).Count;
        Console.WriteLine($"check A->B->A: panel shows {shown.Count} (expected {expected}), distinct {shown.Distinct().Count()}"
            + (shown.Count == expected && shown.Distinct().Count() == expected ? " - ok" : " - FAILED"));
        Frame(window, "A->B->A");

        if (shown.Count == 0)
            return;
        var viewed = shown[0];
        // The viewer is created inside the command; catch it as it opens (headless has no window list).
        Window viewer = null;
        EventHandler<Avalonia.Interactivity.RoutedEventArgs> onOpened = (sender, _) => viewer ??= sender as TweakerUI.Views.ImageViewWindow;
        Window.WindowOpenedEvent.AddClassHandler(typeof(Window), onOpened);
        viewed.ShowBigPictureCommand.Execute(null);
        Pump(() => true);
        vm.SelectedItem = b;
        Pump(() => !vm.IsLoadingScreenshots);
        var keptWhileOpen = !IsDisposed(viewed.Image);
        var others = shown.Skip(1).All(s => IsDisposed(s.Image));

        // In the app the viewer closes itself when it loses activation; headless doesn't move activation
        // between windows, so close it directly.
        viewer?.Close();
        Pump(() => true);
        var releasedOnClose = IsDisposed(viewed.Image);
        Console.WriteLine($"check viewer: kept while open {(keptWhileOpen ? "ok" : "FAILED")}, rest disposed {(others ? "ok" : "FAILED")}, released on close {(releasedOnClose ? "ok" : "FAILED")}");
    }

    // mem [fast] [rounds=N]
    public static void Run(string[] args)
    {
        var fast = args.Contains("fast");
        var appDir = Path.GetDirectoryName(Environment.ProcessPath);
        if (!Directory.Exists(Path.Combine(appDir, "Skins")))
        {
            Console.WriteLine($"no Skins\\ next to {Environment.ProcessPath}");
            return;
        }
        var rounds = args.Where(a => a.StartsWith("rounds=")).Select(a => int.Parse(a[7..])).DefaultIfEmpty(1).First();

        // The constructor reads the installed-skin state from the textures folder - give it an empty one.
        var textures = Path.Combine(Path.GetTempPath(), "UiStand-textures");
        Directory.CreateDirectory(textures);
        SetSetting("GameTexturesPath", textures);
        SetSetting("UseFastPreview", fast);

        // Anything the packager swallows (a failed decode or conversion) would otherwise look like "no screenshots".
        TweakerCore.Engine.SkinPackager.OperationFailed += (message, ex) => Console.WriteLine($"  !! {message}: {ex.GetBaseException().GetType().Name}: {ex.GetBaseException().Message}");

        Report("start", true);

        var sw = Stopwatch.StartNew();
        var vm = new SkinChangerViewModel();
        var window = new Window { Width = 1280, Height = 720, Content = new TweakerUI.Views.SkinChangerView { DataContext = vm } };
        window.Show();
        Pump(() => !vm.LoadingVisible && (!fast || vm.Skins.All(c => c.Screenshots != null)));
        Console.WriteLine($"loaded {vm.Skins.Count} skins in {sw.ElapsedMilliseconds} ms (fast={fast})");
        Frame(window, "load");
        Report("list loaded");
        Idle(1500); // Utils.DisposeAndClear's own delayed GC
        Report("list loaded +1.5s", true);
        Console.WriteLine($"  pixels held by cards/selection: {Mb(HeldPixels(vm))} MB");

        for (int r = 0; r < rounds; r++)
        {
            foreach (var card in vm.Skins.ToList())
            {
                sw.Restart();
                vm.SelectedItem = card;
                Pump(() => !vm.IsLoadingScreenshots);
                Frame(window, "select " + card.Name);
                Report($"select {card.Name,-18} {sw.ElapsedMilliseconds,5}ms");
            }
        }
        Report("after selecting all", true);
        Checks(vm, window);
        Console.WriteLine($"  pixels held by cards/selection: {Mb(HeldPixels(vm))} MB");

        // Fast Previews switched at runtime, as from Settings: on (unless it already is) then off, `cycles` times.
        var cycles = args.Where(a => a.StartsWith("cycles=")).Select(a => int.Parse(a[7..])).DefaultIfEmpty(1).First();
        var setFast = typeof(SkinChangerViewModel).GetMethod("SetFastPreview", BindingFlags.Instance | BindingFlags.NonPublic);
        for (int c = 0; c < cycles; c++)
        {
            if (!fast || c > 0)
            {
                sw.Restart();
                SetSetting("UseFastPreview", true);
                setFast.Invoke(vm, new object[] { true });
                Pump(() => vm.Skins.All(card => card.Screenshots != null));
                Console.WriteLine($"fast previews filled in {sw.ElapsedMilliseconds} ms");
                Frame(window, "fast on");
                Report($"fast on #{c + 1}");
                Idle(1500);
                Report($"fast on #{c + 1} +1.5s", true);
                Console.WriteLine($"  pixels held by cards/selection: {Mb(HeldPixels(vm))} MB");
            }

            SetSetting("UseFastPreview", false);
            setFast.Invoke(vm, new object[] { false });
            Pump(() => !vm.IsLoadingScreenshots);
            Frame(window, "fast off");
            Report($"fast off #{c + 1}");
            Idle(1500);
            Report($"fast off #{c + 1} +1.5s", true);
            Console.WriteLine($"  pixels held by cards/selection: {Mb(HeldPixels(vm))} MB");
        }

        window.Close();
    }
}

// Tab switching the way MainWindow does it: ContentControl + ViewLocator build a NEW view on every switch.
// Counts views still alive after a full GC - anything above one per tab is a view rooted by something.
static class TabLeak
{
    public static void Run()
    {
        var textures = System.IO.Path.Combine(System.IO.Path.GetTempPath(), "UiStand-textures");
        System.IO.Directory.CreateDirectory(textures);
        typeof(SkinChangerViewModel).Assembly.GetType("TweakerUI.Core.SettingsProvider")
            .GetProperty("GameTexturesPath", BindingFlags.Static | BindingFlags.NonPublic).SetValue(null, textures);

        var vms = new List<object>();
        foreach (var t in new[] { "SkinChangerViewModel", "ColorsConfiguratorViewModel", "TweakerViewModel", "QuickPlayerViewModel", "SettingViewModel" })
        {
            try { vms.Add(Activator.CreateInstance(typeof(SkinChangerViewModel).Assembly.GetType("TweakerUI.ViewModels." + t))); }
            catch (Exception ex) { Console.WriteLine($"{t}: {ex.GetBaseException().Message}"); }
        }

        var host = new ContentControl();
        var window = new Window { Width = 1280, Height = 720, Content = host };
        window.DataTemplates.Add(new TweakerUI.ViewLocator());
        window.Show();

        var views = new List<WeakReference>();
        var names = new List<string>();
        for (int round = 0; round < 20; round++)
            foreach (var vm in vms)
            {
                host.Content = vm;
                for (int i = 0; i < 5; i++) Dispatcher.UIThread.RunJobs();
                var view = host.Presenter?.Child;
                views.Add(new WeakReference(view));
                names.Add(view?.GetType().Name);
            }
        host.Content = null;
        for (int i = 0; i < 5; i++) Dispatcher.UIThread.RunJobs();
        for (int i = 0; i < 3; i++) { GC.Collect(); GC.WaitForPendingFinalizers(); }

        foreach (var g in views.Zip(names).GroupBy(p => p.Second))
            Console.WriteLine($"{g.Key,-28} built {g.Count(),3}, alive after GC {g.Count(p => p.First.IsAlive),3}");
        window.Close();
    }
}
