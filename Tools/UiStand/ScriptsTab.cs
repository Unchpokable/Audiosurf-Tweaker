using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using Avalonia.Controls;
using Avalonia.Headless;
using Avalonia.Styling;
using Avalonia.Threading;
using TweakerUI.Core;
using TweakerUI.Models;
using TweakerUI.ViewModels;
using Application = Avalonia.Application;

// The Scripts tab (script-manager-roadmap.md, С3) with its REAL view model over a fake game folder: every kind
// of row (bundled, edited, yours, project build with and without its map, not installed, with .old copies),
// the notices, the empty states - both themes, both module widths. Then the switch: flipping one must write
// scripts.cfg the way the plugin reads it.
//
// The bundle is PluginPayload\ next to UiStand.exe (PluginService looks there, like the app does next to
// TweakerUI.exe), created for the run and removed after it.
static class ScriptsTab
{
    static readonly Type Settings = typeof(ScriptsViewModel).Assembly.GetType("TweakerUI.Core.SettingsProvider");

    static void SetTexturesPath(string path) =>
        Settings.GetProperty("GameTexturesPath", BindingFlags.Static | BindingFlags.NonPublic).SetValue(null, path);

    public static int Run(string outDir)
    {
        Directory.CreateDirectory(outDir);
        var root = Path.Combine(Path.GetTempPath(), "UiStand-scripts-" + Guid.NewGuid().ToString("N"));
        var engine = Path.Combine(root, "Audiosurf", "engine");
        var payload = Path.Combine(Path.GetDirectoryName(Environment.ProcessPath)!, "PluginPayload");
        var failures = 0;

        try
        {
            BuildGame(engine, payload);
            SetTexturesPath(Path.Combine(engine, "textures"));

            foreach (var theme in new[] { ThemeVariant.Light, ThemeVariant.Dark })
            {
                Application.Current!.RequestedThemeVariant = theme;
                foreach (var width in new[] { 738, 1658 })
                    Shoot(outDir, $"{theme}-scripts-{width}", width, vm => { }, 1000);
                Shoot(outDir, $"{theme}-scripts-game-running", 738, vm =>
                {
                    vm.GameRunning = true;
                    foreach (var row in vm.Scripts)
                        row.CanToggle = false;
                    typeof(ScriptsViewModel).GetMethod("UpdateNotice", BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(vm, null);
                });
            }

            Application.Current!.RequestedThemeVariant = ThemeVariant.Light;

            // No plugin in the game: the notice says so, the list is still there.
            File.Move(Path.Combine(engine, "channels", "TweakerPlugin.dll"), Path.Combine(engine, "channels", "TweakerPlugin.dll.off"));
            Shoot(outDir, "Light-scripts-no-plugin", 738, vm => { });
            File.Move(Path.Combine(engine, "channels", "TweakerPlugin.dll.off"), Path.Combine(engine, "channels", "TweakerPlugin.dll"));

            // No game folder at all.
            SetTexturesPath(null);
            Shoot(outDir, "Light-scripts-no-game", 738, vm => { });
            SetTexturesPath(Path.Combine(engine, "textures"));

            failures += CheckToggle(engine);
            failures += CheckLive(outDir, engine);
        }
        finally
        {
            ScriptsOverlayBridge.UseTransport(_ => false, () => false);
            try { Directory.Delete(root, true); } catch (IOException) { }
            try { Directory.Delete(payload, true); } catch (IOException) { }
        }

        Console.WriteLine(failures == 0 ? $"scripts: ok, snapshots in {outDir}" : $"scripts: {failures} check(s) FAILED");
        return failures == 0 ? 0 : 1;
    }

    static ScriptsViewModel Load()
    {
        var vm = new ScriptsViewModel();
        vm.SetActive(true);
        var clock = Stopwatch.StartNew();
        // The scan runs on the thread pool and lands back through the dispatcher.
        while (clock.ElapsedMilliseconds < 5000)
        {
            Dispatcher.UIThread.RunJobs();
            if (vm.Summary != null || vm.EmptyText != null)
                break;
            System.Threading.Thread.Sleep(10);
        }
        Dispatcher.UIThread.RunJobs();
        return vm;
    }

    static void Shoot(string outDir, string name, int width, Action<ScriptsViewModel> arrange, int height = 665)
    {
        var vm = Load();
        arrange(vm);
        var w = new Window { Width = width, Height = height, Content = new TweakerUI.Views.ScriptsView { DataContext = vm } };
        w.Show();
        for (var i = 0; i < 5; i++)
            Dispatcher.UIThread.RunJobs();
        using (var frame = w.CaptureRenderedFrame())
            frame!.Save(Path.Combine(outDir, name + ".png"));
        w.Close();
        vm.SetActive(false);
        vm.Dispose();
    }

    static int CheckToggle(string engine)
    {
        var failures = 0;
        var vm = Load();
        var row = vm.Scripts.Single(r => r.FileName == "my_hud.lua");
        var cfg = Path.Combine(engine, "TweakerStuff", "Config", "scripts.cfg");
        var before = File.ReadAllText(cfg);
        row.Enabled = false;
        var text = File.ReadAllText(cfg);

        // A real Audiosurf running on this machine is a real reason to refuse - the plugin owns the file then.
        // The stand checks the refusal instead; the write path needs the game closed.
        if (vm.GameRunning)
        {
            Dispatcher.UIThread.RunJobs();
            if (text != before || row.Enabled == false)
            {
                Console.WriteLine("FAIL with Audiosurf running, switching wrote scripts.cfg or the switch stayed moved");
                failures++;
            }
            Console.WriteLine("note: Audiosurf is running - checked that switching is refused; close it to check the write");
            vm.SetActive(false);
            vm.Dispose();
            return failures;
        }

        if (!text.Contains("script.my_hud.lua=0\n") || !text.Contains("engine.settle_ms=900"))
        {
            Console.WriteLine("FAIL switching off did not write script.my_hud.lua=0 while keeping engine.settle_ms:\n" + text);
            failures++;
        }
        if (vm.Summary != "4 of 6 enabled")
        {
            Console.WriteLine($"FAIL summary after switching off: '{vm.Summary}'");
            failures++;
        }

        row.Enabled = true;
        text = File.ReadAllText(cfg);
        if (text.Contains("my_hud"))
        {
            Console.WriteLine("FAIL switching on left a line for the script:\n" + text);
            failures++;
        }
        vm.SetActive(false);
        vm.Dispose();
        return failures;
    }

    // --- С5b: the game connected -------------------------------------------------------------------------------
    //
    // A fake plugin behind ScriptsOverlayBridge: every line the tab sends is recorded, and the plugin's answers
    // are fed in by hand. The game counts as running throughout - that is when the link exists - so the switches
    // move only because of it, and scripts.cfg must not be touched by the host at all.

    static int CheckLive(string outDir, string engine)
    {
        var failures = 0;
        void Check(bool ok, string what)
        {
            if (ok)
                return;
            Console.WriteLine("FAIL " + what);
            failures++;
        }

        void Pump(int ms = 0)
        {
            var clock = Stopwatch.StartNew();
            do
            {
                Dispatcher.UIThread.RunJobs();
                System.Threading.Thread.Sleep(5);
            } while (clock.ElapsedMilliseconds < ms);
            Dispatcher.UIThread.RunJobs();
        }

        var cfg = Path.Combine(engine, "TweakerStuff", "Config", "scripts.cfg");
        var cfgBefore = File.ReadAllText(cfg);
        var sent = new System.Collections.Generic.List<string>();
        var connected = true;
        var timedOut = new System.Collections.Generic.List<string>();
        EventHandler<string> onTimeout = (_, file) => timedOut.Add(file);
        ScriptsOverlayBridge.RequestTimedOut += onTimeout;
        ScriptsOverlayBridge.ConfirmTimeout = TimeSpan.FromSeconds(30);
        ScriptsOverlayBridge.UseTransport(line => { sent.Add(line); return true; }, () => connected);

        // The handshake's list, as lua_remote sends it.
        foreach (var line in new[]
                 {
                     "SCRIPT_STATE lost_map.lua true suspended 5%20errors%20in%2012%20frames%2C%20the%20last%3A%20lost_map.lua%3A1%3A%20attempt%20to%20call%20a%20nil%20value",
                     "SCRIPT_STATE my_hud.lua true running",
                     "SCRIPT_STATE old_thing.lua false off",
                     "SCRIPT_STATE puzzlepro_hud.lua true running",
                     "SCRIPT_STATE quick_probe.lua false missing",
                     "SCRIPT_STATE ring_pulse.lua true failed ring_pulse.lua%3A3%3A%20attempt%20to%20index%20a%20nil%20value%20%28field%20%27rings%27%29",
                     "SCRIPT_SYNCED 6",
                 })
            ScriptsOverlayBridge.Receive(line);

        var vm = Load();
        vm.GameRunning = true;
        ScriptsOverlayBridge.Receive("SCRIPT_STATE my_hud.lua true running"); // a change, so the tab re-lays everything with the game up
        Pump();

        ScriptRowViewModel Row(string file) => vm.Scripts.Single(r => r.FileName == file);

        Check(vm.Live && ScriptsOverlayBridge.IsSynced, "the tab is live after the list");
        Check(vm.Summary.EndsWith("live from the game"), "the summary says the game is live: " + vm.Summary);
        Check(!vm.HasNotice || !vm.Notice.Contains("not connected"), "no 'not connected' notice while connected: " + vm.Notice);
        Check(Row("ring_pulse.lua").LiveFailed && Row("ring_pulse.lua").LiveText == "Failed: ring_pulse.lua:3: attempt to index a nil value (field 'rings')",
            "a failed script shows its error: " + Row("ring_pulse.lua").LiveText);
        Check(Row("lost_map.lua").LiveSuspended && Row("lost_map.lua").LiveText.StartsWith("Suspended: 5 errors in 12 frames"),
            "a suspended script shows why: " + Row("lost_map.lua").LiveText);
        Check(Row("quick_probe.lua").LiveText.Contains("has not picked this file up"), "a file the game has not seen says so: " + Row("quick_probe.lua").LiveText);
        Check(!Row("my_hud.lua").HasLiveText, "a running script needs no caption");
        Check(Row("my_hud.lua").CanToggle && !Row("song_meter.lua").CanToggle, "switches move with the game up - except for a script that is not installed");

        // Pending, as it looks: one row waiting, one failed, one suspended.
        Row("puzzlepro_hud.lua").Enabled = false;
        Pump();
        Check(sent.LastOrDefault() == "SCRIPT_SET puzzlepro_hud.lua false", "the switch asks the plugin: " + sent.LastOrDefault());
        Check(Row("puzzlepro_hud.lua").IsPending && !Row("puzzlepro_hud.lua").Enabled && Row("puzzlepro_hud.lua").LiveText == "Waiting for the game...",
            "and shows the new position, waiting");
        Check(File.ReadAllText(cfg) == cfgBefore, "the host does not write scripts.cfg while the game is connected");
        foreach (var theme in new[] { ThemeVariant.Light, ThemeVariant.Dark })
        {
            Application.Current!.RequestedThemeVariant = theme;
            ShootLive(outDir, $"{theme}-scripts-live", vm);
        }
        Application.Current!.RequestedThemeVariant = ThemeVariant.Light;

        ScriptsOverlayBridge.Receive("SCRIPT_STATE puzzlepro_hud.lua false off");
        Pump();
        Check(!Row("puzzlepro_hud.lua").IsPending && !Row("puzzlepro_hud.lua").Enabled && !Row("puzzlepro_hud.lua").HasLiveText,
            "the plugin's answer settles it");

        // An enable that does not take: the plugin answers "false failed", and the switch goes back with the reason.
        Row("old_thing.lua").Enabled = true;
        Pump();
        ScriptsOverlayBridge.Receive("SCRIPT_STATE old_thing.lua false failed old_thing.lua%3A2%3A%20boom");
        Pump();
        Check(!Row("old_thing.lua").Enabled && Row("old_thing.lua").LiveFailed && Row("old_thing.lua").LiveText == "Failed: old_thing.lua:2: boom",
            "a failed enable springs back with its error: " + Row("old_thing.lua").LiveText);

        // No answer at all: back to the last known position after the timeout, and the user is told.
        ScriptsOverlayBridge.ConfirmTimeout = TimeSpan.FromMilliseconds(200);
        Row("my_hud.lua").Enabled = false;
        Pump();
        Check(Row("my_hud.lua").IsPending && !Row("my_hud.lua").Enabled, "an unanswered switch waits");
        Pump(500);
        Check(!Row("my_hud.lua").IsPending && Row("my_hud.lua").Enabled && timedOut.SequenceEqual(new[] { "my_hud.lua" }),
            "and springs back when nobody answers: pending " + Row("my_hud.lua").IsPending + ", enabled " + Row("my_hud.lua").Enabled + ", timed out [" + string.Join(", ", timedOut) + "]");

        // Last click wins: a second click replaces the first, and the first one's clock does not fire on it.
        ScriptsOverlayBridge.ConfirmTimeout = TimeSpan.FromMilliseconds(600);
        Row("my_hud.lua").Enabled = false;
        Pump(300);
        Row("my_hud.lua").Enabled = true;
        Pump(400); // past the first click's deadline, short of the second's
        Check(Row("my_hud.lua").IsPending && Row("my_hud.lua").Enabled, "a second click restarts the wait");
        Check(sent.Skip(sent.Count - 2).SequenceEqual(new[] { "SCRIPT_SET my_hud.lua false", "SCRIPT_SET my_hud.lua true" }), "and both clicks went out");
        ScriptsOverlayBridge.Receive("SCRIPT_STATE my_hud.lua true running");
        Pump();

        // Refresh is also "rescan in the game".
        sent.Clear();
        vm.RefreshCommand.Execute(null);
        Pump();
        Check(sent.SequenceEqual(new[] { "SCRIPT_RESCAN" }), "Refresh asks the game to rescan: [" + string.Join(", ", sent) + "]");
        Check(!ScriptsOverlayBridge.IsSynced, "and waits for the list again");

        // The link goes: everything the game said is forgotten, and with the game still up the switches lock.
        connected = false;
        ScriptsOverlayBridge.RefreshLink();
        Pump();
        Check(!vm.Live && !Row("ring_pulse.lua").HasLiveText && !Row("my_hud.lua").CanToggle, "losing the link forgets the game's word and locks the switches");
        Check(vm.HasNotice && vm.Notice.Contains("not connected"), "and the notice says why: " + vm.Notice);

        ScriptsOverlayBridge.RequestTimedOut -= onTimeout;
        ScriptsOverlayBridge.ConfirmTimeout = TimeSpan.FromSeconds(5);
        vm.SetActive(false);
        vm.Dispose();
        return failures;
    }

    static void ShootLive(string outDir, string name, ScriptsViewModel vm)
    {
        var w = new Window { Width = 738, Height = 1000, Content = new TweakerUI.Views.ScriptsView { DataContext = vm } };
        w.Show();
        for (var i = 0; i < 5; i++)
            Dispatcher.UIThread.RunJobs();
        using (var frame = w.CaptureRenderedFrame())
            frame!.Save(Path.Combine(outDir, name + ".png"));
        w.Close();
    }

    // --- the fake game --------------------------------------------------------------------------------------

    static void BuildGame(string engine, string payload)
    {
        Directory.CreateDirectory(Path.Combine(engine, "channels"));
        Directory.CreateDirectory(Path.Combine(engine, "textures"));
        File.WriteAllText(Path.Combine(engine, "QuestViewer.exe"), "not really the game");
        File.WriteAllText(Path.Combine(engine, "channels", "TweakerPlugin.dll"), "plugin");
        var scripts = Path.Combine(engine, "TweakerStuff", "Scripts");
        Directory.CreateDirectory(scripts);

        var bundled = Path.Combine(payload, "TweakerStuff", "Scripts");
        Directory.CreateDirectory(bundled);

        const string hud = "-- @name PuzzlePro HUD\n-- @author Unchpokable\n-- @version 1.4.0\n-- @description Puzzle grid, chain counter and match hints drawn over the track.\n-- @api 1\n-- @bundle twllink 1.1, 17 modules\nlocal x = 1\n";
        const string rings = "-- @name Ring Pulse\n-- @version 0.3.0\n-- @description Pulses the rings with the bass.\nlocal x = 1\n";
        const string meter = "-- @name Song Meter\n-- @description Shows where in the song you are.\nlocal x = 1\n";
        File.WriteAllText(Path.Combine(bundled, "puzzlepro_hud.lua"), hud);
        File.WriteAllText(Path.Combine(bundled, "puzzlepro_hud.lua.map"), "# twllink 1\n");
        File.WriteAllText(Path.Combine(bundled, "ring_pulse.lua"), rings);
        File.WriteAllText(Path.Combine(bundled, "song_meter.lua"), meter);

        // Installed: the HUD as shipped, the ring pulse edited since, the song meter deleted by the user.
        File.WriteAllText(Path.Combine(scripts, "puzzlepro_hud.lua"), hud);
        File.WriteAllText(Path.Combine(scripts, "puzzlepro_hud.lua.map"), "# twllink 1\n");
        File.WriteAllText(Path.Combine(scripts, "ring_pulse.lua"), rings.Replace("0.3.0", "0.3.0-mine"));
        File.WriteAllText(Path.Combine(scripts, "ring_pulse.lua.old"), "-- an older edit\n");
        File.WriteAllText(Path.Combine(scripts, "ring_pulse.lua.old.2"), "-- another\n");
        File.WriteAllText(Path.Combine(engine, "TweakerStuff", ".deploy.json"),
            "{ \"plugin_version\": null, \"files\": {"
            + $" \"TweakerStuff/Scripts/puzzlepro_hud.lua\": {{ \"sha256\": \"{Hash(hud)}\" }},"
            + $" \"TweakerStuff/Scripts/ring_pulse.lua\": {{ \"sha256\": \"{Hash(rings)}\" }},"
            + $" \"TweakerStuff/Scripts/song_meter.lua\": {{ \"sha256\": \"{Hash(meter)}\" }} }} }}");

        // The user's own: one built from a project (with its map), one built but without the map, one by hand
        // with no header at all, one switched off.
        File.WriteAllText(Path.Combine(scripts, "my_hud.lua"), "-- @name My HUD\n-- @author you\n-- @version 0.1.0\n-- @api 1\n-- @bundle twllink 1.1, 3 modules\nlocal x = 1\n");
        File.WriteAllText(Path.Combine(scripts, "my_hud.lua.map"), "# twllink 1\n");
        File.WriteAllText(Path.Combine(scripts, "lost_map.lua"), "-- @name Lost Map\n-- @bundle twllink 1.1, 2 modules\nlocal x = 1\n");
        File.WriteAllText(Path.Combine(scripts, "quick_probe.lua"), "local x = 1\n");
        File.WriteAllText(Path.Combine(scripts, "old_thing.lua"), "-- @name Old Thing\n-- @description Switched off in the overlay.\nlocal x = 1\n");

        Directory.CreateDirectory(Path.Combine(engine, "TweakerStuff", "Config"));
        File.WriteAllText(Path.Combine(engine, "TweakerStuff", "Config", "scripts.cfg"),
            "# Scripts the overlay should not run.\nscript.old_thing.lua=0\n\nengine.settle_ms=900\n");
    }

    static string Hash(string text) => Convert.ToHexString(SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(text))).ToLowerInvariant();
}
