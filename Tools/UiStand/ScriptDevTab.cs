using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Headless;
using Avalonia.Styling;
using Avalonia.Threading;
using TweakerUI.Models;
using TweakerUI.ViewModels;
using Application = Avalonia.Application;

// The Script Dev tab (script-manager-roadmap.md, С4a) with its REAL view model and the REAL twllink: create a
// project from a template, a bad name, build, a build with an error, install into a fake game, and rebuild on
// save - one rebuild per change, none for twllink's own writes into build\. Snapshots of each state, both themes.
//
// DevTools\ is next to UiStand.exe, as it is next to TweakerUI.exe: twllink is copied there from TweakerUI's
// build output, Sources\ and recent-projects.txt are created for the run and removed after it.
static class ScriptDevTab
{
    static readonly Type Settings = typeof(ScriptDevViewModel).Assembly.GetType("TweakerUI.Core.SettingsProvider");
    static int _failures;

    static void Check(bool ok, string what)
    {
        if (ok)
            return;
        Console.WriteLine("FAIL " + what);
        _failures++;
    }

    public static int Run(string outDir)
    {
        Directory.CreateDirectory(outDir);
        var exeDir = Path.GetDirectoryName(Environment.ProcessPath)!;
        var devTools = Path.Combine(exeDir, "DevTools");
        var uiBin = Path.GetFullPath(Path.Combine(exeDir, "..", "..", "..", "..", "..", "..", "TweakerUI", "bin", "x64", "Debug", "net10.0-windows10.0.19041.0"));
        var root = Path.Combine(Path.GetTempPath(), "UiStand-scriptdev-" + Guid.NewGuid().ToString("N"));
        var engine = Path.Combine(root, "Audiosurf", "engine");

        try
        {
            if (Directory.Exists(devTools))
                Directory.Delete(devTools, true);
            var binaries = Path.Combine(devTools, "Binaries");
            Directory.CreateDirectory(binaries);
            foreach (var file in Directory.GetFiles(Path.Combine(uiBin, "DevTools", "Binaries")))
                File.Copy(file, Path.Combine(binaries, Path.GetFileName(file)));

            Directory.CreateDirectory(Path.Combine(engine, "channels"));
            Directory.CreateDirectory(Path.Combine(engine, "textures"));
            Directory.CreateDirectory(Path.Combine(engine, "TweakerStuff", "Scripts"));
            File.WriteAllText(Path.Combine(engine, "QuestViewer.exe"), "not really the game");
            Settings.GetProperty("GameTexturesPath", BindingFlags.Static | BindingFlags.NonPublic).SetValue(null, Path.Combine(engine, "textures"));

            var vm = new ScriptDevViewModel();
            vm.SetActive(true);
            Check(!vm.HasProjects, "a fresh DevTools lists no projects");
            ShootBoth(outDir, "scriptdev-empty", vm);

            // The two names: the id follows the name as it is typed, until the id is typed itself.
            vm.StartCreateCommand.Execute(null);
            vm.NewName = "Stand HUD!";
            Check(vm.NewId == "stand_hud", $"the id follows the name ({vm.NewId})");
            ShootBoth(outDir, "scriptdev-new", vm);
            vm.NewId = "Stand HUD";
            vm.NewName = "Stand HUD, renamed";
            Check(vm.NewId == "Stand HUD", "a typed id stops following the name");
            Await(vm.CreateCommand.ExecuteAsync(null));
            Check(vm.NewIdProblem != null && vm.NewNameProblem == null && vm.IsCreating, "an id with capitals and spaces is refused, the name is fine");
            ShootBoth(outDir, "scriptdev-new-bad-id", vm);

            vm.NewId = "stand_hud";
            vm.NewName = "Stand HUD";
            vm.NewDescription = "Shows the stand's score.";
            vm.NewTemplate = vm.Templates.Single(t => t.Key == "hud");
            Await(vm.CreateCommand.ExecuteAsync(null));
            Check(!vm.IsCreating, "the form closes once the project is created: " + vm.CreateProblem + vm.NewIdProblem);
            Check(vm.Selected?.Project.Id == "stand_hud" && vm.Selected.Title == "Stand HUD" && vm.Selected.Description == "Shows the stand's score.",
                $"the new project is selected, with the name and description typed ({vm.Selected?.Title} / {vm.Selected?.Description})");
            var project = vm.Selected!.Root;
            Check(File.Exists(Path.Combine(devTools, "recent-projects.txt")), "a new project is remembered");

            // A second project, from the other template, so the list has more than one row.
            vm.StartCreateCommand.Execute(null);
            vm.NewName = "Stand Hook";
            vm.NewTemplate = vm.Templates.Single(t => t.Key == "hook");
            Await(vm.CreateCommand.ExecuteAsync(null));
            Check(vm.Selected?.Project.Id == "stand_hook", "the hook template creates its project");

            // Picking another project leaves an open form.
            vm.StartCreateCommand.Execute(null);
            vm.Selected = vm.Projects.Single(p => p.Project.Id == "stand_hud");
            Check(!vm.IsCreating && vm.ShowDetails, "selecting a project closes the new-project form");

            CheckEditing(vm, project, outDir);

            Await(vm.BuildCommand.ExecuteAsync(null));
            Check(vm.Selected.Succeeded && File.Exists(Path.Combine(project, "build", "stand_hud.lua")), "a new project builds: " + vm.Selected.BuildStatus);
            ShootBoth(outDir, "scriptdev-built", vm);

            File.WriteAllText(Path.Combine(project, "src", "main.lua"), "local f = io.open('x')\nlocal y = tw.hud.txt\n");
            Await(vm.BuildCommand.ExecuteAsync(null));
            Check(vm.Selected.Failed && vm.Selected.Diagnostics.Any(d => d.Code == "TW0301" && d.Location == "src/main.lua:1:11"),
                "an error comes back as a diagnostic with its place: " + string.Join("; ", vm.Selected.Diagnostics.Select(d => d.Location + " " + d.Code)));
            ShootBoth(outDir, "scriptdev-failed", vm);

            File.WriteAllText(Path.Combine(project, "src", "main.lua"), "local x = 1\n");
            Await(vm.InstallCommand.ExecuteAsync(null));
            Check(vm.Selected.Succeeded && File.Exists(Path.Combine(engine, "TweakerStuff", "Scripts", "stand_hud.lua")),
                "build and install puts the script into the game: " + vm.Selected.BuildStatus + " / " + vm.Selected.BuildDetail);
            ShootBoth(outDir, "scriptdev-installed", vm);

            // С5b: installing into a connected game asks it to reload the script; without the link nothing is sent
            // (the install above went out with no fake plugin attached).
            var sent = new System.Collections.Generic.List<string>();
            TweakerUI.Core.ScriptsOverlayBridge.UseTransport(line => { sent.Add(line); return true; }, () => true);
            Await(vm.InstallCommand.ExecuteAsync(null));
            Check(sent.SequenceEqual(new[] { "SCRIPT_RELOAD stand_hud.lua" }) && vm.Selected.BuildStatus.StartsWith("Built, installed and reloaded"),
                "installing into a connected game reloads it there: [" + string.Join(", ", sent) + "] / " + vm.Selected.BuildStatus);
            sent.Clear();
            Await(vm.BuildCommand.ExecuteAsync(null));
            Check(sent.Count == 0, "a build without install sends nothing: [" + string.Join(", ", sent) + "]");
            TweakerUI.Core.ScriptsOverlayBridge.UseTransport(_ => false, () => false);

            CheckRebuildOnSave(vm, project);
            CheckNeovimSeesTwllink(project, binaries);

            // A remembered project whose folder went away, and one with a broken manifest.
            var gone = Path.Combine(root, "gone_hud");
            Directory.CreateDirectory(gone);
            File.WriteAllText(Path.Combine(gone, "twproject.json"), "{ \"id\": \"gone_hud\" }");
            var broken = Path.Combine(root, "broken");
            Directory.CreateDirectory(broken);
            File.WriteAllText(Path.Combine(broken, "twproject.json"), "{ not json");
            var projects = typeof(ScriptDevViewModel).GetField("_projects", BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(vm)!;
            projects.GetType().GetMethod("Remember")!.Invoke(projects, new object[] { gone });
            projects.GetType().GetMethod("Remember")!.Invoke(projects, new object[] { broken });
            Directory.Delete(gone, true);
            vm.RefreshCommand.Execute(null);
            Check(vm.Projects.Any(p => p.IsMissing) && vm.Projects.Any(p => p.HasProblem), "a missing and a broken project are listed");
            vm.Selected = vm.Projects.Single(p => p.IsMissing);
            Check(vm.ForgetCommand.CanExecute(null), "a missing project opened from elsewhere can be removed from the list");
            vm.ForgetCommand.Execute(null);
            Check(!vm.Projects.Any(p => p.IsMissing), "forgotten, it is gone");
            vm.Selected = vm.Projects.Single(p => p.HasProblem);
            ShootBoth(outDir, "scriptdev-problems", vm);

            // The owner's case: a project made here, its folder deleted by hand - gone on Refresh, not "missing" forever.
            var hook = vm.Projects.Single(p => p.Project.Id == "stand_hook").Root;
            Directory.Delete(hook, true);
            vm.RefreshCommand.Execute(null);
            Check(!vm.Projects.Any(p => p.Root == hook), "a deleted project from Sources disappears on Refresh");

            // And a line removed from recent-projects.txt by hand, without a restart.
            File.WriteAllText(Path.Combine(devTools, "recent-projects.txt"), "");
            vm.RefreshCommand.Execute(null);
            Check(!vm.Projects.Any(p => p.HasProblem), "a project removed from the recents file by hand disappears on Refresh");

            vm.Dispose();
        }
        finally
        {
            try { Directory.Delete(root, true); } catch (IOException) { }
            try { Directory.Delete(devTools, true); } catch (IOException) { }
        }

        Console.WriteLine(_failures == 0 ? $"scriptdev: ok, snapshots in {outDir}" : $"scriptdev: {_failures} check(s) FAILED");
        return _failures == 0 ? 0 : 1;
    }

    /// <summary>
    /// The manifest form: a bad version blocks Save, a good edit changes exactly those fields in twproject.json and
    /// keeps a comment the user wrote there; a new id warns about the copy already in the game.
    /// </summary>
    static void CheckEditing(ScriptDevViewModel vm, string project, string outDir)
    {
        var manifest = Path.Combine(project, "twproject.json");
        File.WriteAllText(manifest, File.ReadAllText(manifest).Replace("\"format\": 1,", "// the owner's note\n  \"format\": 1,"));
        vm.RefreshCommand.Execute(null);

        vm.StartEditCommand.Execute(null);
        Check(vm.IsEditing && vm.EditName == "Stand HUD" && vm.EditVersion == "0.1.0", "editing starts from the manifest's values");
        vm.EditVersion = "1.0";
        Check(vm.EditVersionProblem != null && !vm.SaveEditCommand.CanExecute(null), "a bad version blocks Save");
        vm.EditId = "stand_hud_two";
        Check(vm.IdChangeWarning != null, "a new id warns about the copy in the game");
        ShootBoth(outDir, "scriptdev-editing", vm);

        CheckDescriptionField(vm, outDir);

        vm.EditId = "stand_hud";
        vm.EditVersion = "1.2.0";
        vm.EditName = "Stand HUD Deluxe";
        vm.EditDescription = "Now with more score.";
        vm.SaveEditCommand.Execute(null);

        var text = File.ReadAllText(manifest);
        Check(!vm.IsEditing && vm.Selected!.Title == "Stand HUD Deluxe" && vm.Selected.Subtitle.Contains("1.2.0"), "saved values show at once");
        Check(text.Contains("\"version\": \"1.2.0\"") && text.Contains("\"name\": \"Stand HUD Deluxe\"") && text.Contains("// the owner's note"),
            "twproject.json has the new values and keeps the comment:\n" + text);

        vm.StartEditCommand.Execute(null);
        vm.CancelEditCommand.Execute(null);
        Check(!vm.IsEditing && File.ReadAllText(manifest) == text, "Cancel changes nothing");
    }

    /// <summary>
    /// The description field: one short line is one line high; a long one grows the field to its limit and scrolls
    /// past it; a pasted paragraph with line breaks becomes one line (the manifest's rule), not an error.
    /// </summary>
    static void CheckDescriptionField(ScriptDevViewModel vm, string outDir)
    {
        double Height(string text, out double extent, out double viewport)
        {
            vm.EditDescription = text;
            var w = new Window { Width = 938, Height = 900, Content = new TweakerUI.Views.ScriptDevView { DataContext = vm } };
            w.Show();
            for (var i = 0; i < 5; i++)
                Dispatcher.UIThread.RunJobs();
            var box = Avalonia.VisualTree.VisualExtensions.GetVisualDescendants(w).OfType<TextBox>().Single(t => t.Classes.Contains("TextArea") && t.IsEffectivelyVisible);
            var scroll = Avalonia.VisualTree.VisualExtensions.GetVisualDescendants(box).OfType<ScrollViewer>().Single();
            extent = scroll.Extent.Height;
            viewport = scroll.Viewport.Height;
            var height = box.Bounds.Height;
            w.Close();
            return height;
        }

        var shortHeight = Height("Short.", out _, out _);
        var midHeight = Height(string.Join(" ", Enumerable.Repeat("A description that goes on for a while.", 12)), out var midExtent, out var midViewport);
        var longHeight = Height(string.Join(" ", Enumerable.Repeat("A description that goes on and on and on for a long while.", 40)), out var longExtent, out var longViewport);
        Console.WriteLine($"description field: short {shortHeight:F0}px, 12 sentences {midHeight:F0}px, 40 sentences {longHeight:F0}px (content {longExtent:F0}px)");
        Check(shortHeight < 50, $"a short description is one line high ({shortHeight:F0}px)");
        Check(midHeight > shortHeight * 2 && midHeight <= 200 && midExtent <= midViewport + 1, $"a longer one grows the field without scrolling ({midHeight:F0}px)");
        Check(longHeight <= 200.5 && longExtent > longViewport + 20, $"a very long one stops at the limit and scrolls ({longHeight:F0}px, content {longExtent:F0}px)");

        vm.EditDescription = "First paragraph.\r\n\r\nSecond one,\n\twith a tab.";
        Check(vm.EditDescription == "First paragraph. Second one, with a tab." && vm.EditDescriptionProblem == null,
            $"a pasted paragraph becomes one line, not an error: '{vm.EditDescription}'");

        vm.EditDescription = string.Join(" ", Enumerable.Repeat("A description that goes on for a while.", 12));
        ShootBoth(outDir, "scriptdev-editing-long-description", vm);
    }

    /// <summary>
    /// One change → exactly one rebuild, after the settle time; twllink's own output (build\, .twllink\) is not a
    /// change, so nothing follows it. Builds are counted by the row entering the Building state.
    /// </summary>
    static void CheckRebuildOnSave(ScriptDevViewModel vm, string project)
    {
        var builds = 0;
        PropertyChangedEventHandler count = (_, e) =>
        {
            if (e.PropertyName == nameof(ScriptProjectRowViewModel.Outcome) && vm.Selected!.Outcome == BuildOutcome.Building)
                builds++;
        };
        vm.Selected!.PropertyChanged += count;
        vm.RebuildOnSave = true;

        Pump(TimeSpan.FromSeconds(1));
        Check(builds == 0, $"switching rebuild-on-save on builds nothing by itself ({builds})");

        File.WriteAllText(Path.Combine(project, "src", "main.lua"), "local x = 2\n");
        Pump(TimeSpan.FromSeconds(3));
        Check(builds == 1, $"one save, one rebuild ({builds})");
        Check(File.ReadAllText(Path.Combine(project, "build", "stand_hud.lua")).Contains("local x = 2"), "the rebuild has the change");

        Pump(TimeSpan.FromSeconds(2));
        Check(builds == 1, $"the rebuild's own output does not trigger another ({builds})");

        vm.RebuildOnSave = false;
        File.WriteAllText(Path.Combine(project, "src", "main.lua"), "local x = 3\n");
        Pump(TimeSpan.FromSeconds(2));
        Check(builds == 1, $"switched off, a save builds nothing ({builds})");
        vm.Selected.PropertyChanged -= count;
    }

    /// <summary>
    /// The one editor whose PATH can be checked without opening a window: Neovim, when it is installed. The start
    /// info is exactly what "Open in Neovim" builds; only the arguments are swapped for a headless run that asks
    /// Neovim's own PATH lookup (the environment :terminal and :! inherit) where twllink is, and reads AUDIOSURF_DIR.
    /// </summary>
    static void CheckNeovimSeesTwllink(string project, string binaries)
    {
        var nvim = TweakerCore.Scripting.ScriptEditors.Detect(TweakerCore.Scripting.EditorSearch.FromSystem()).FirstOrDefault(e => e.Id == "neovim");
        if (nvim == null)
        {
            Console.WriteLine("note: Neovim is not installed - its PATH check is skipped");
            return;
        }

        var info = TweakerCore.Scripting.ScriptEditors.StartInfo(nvim, project, null, 0, 0, binaries, @"D:\Games\Audiosurf");
        info.ArgumentList.Clear();
        info.ArgumentList.Add("--headless");
        info.ArgumentList.Add("--clean");
        info.ArgumentList.Add("+lua io.stdout:write(vim.fn.exepath('twllink') .. '|' .. (vim.env.AUDIOSURF_DIR or '') .. '|' .. vim.trim(vim.fn.system('twllink --version')))");
        info.ArgumentList.Add("+qa!");
        info.RedirectStandardOutput = true;
        info.CreateNoWindow = true;
        // Started from Explorer, TweakerUI has no SHELL and Neovim's :! runs cmd. The stand is usually started from
        // Git Bash, whose SHELL would make it bash instead - not what a user gets, so it goes.
        info.Environment.Remove("SHELL");

        using var process = Process.Start(info)!;
        var output = process.StandardOutput.ReadToEnd();
        process.WaitForExit(20000);
        var parts = output.Split('|');
        Check(parts.Length == 3 && parts[0].StartsWith(Path.Combine(binaries, "twllink"), StringComparison.OrdinalIgnoreCase),
            "Neovim finds twllink in DevTools\\Binaries first: " + output);
        Check(parts.Length == 3 && parts[1] == @"D:\Games\Audiosurf", "Neovim sees AUDIOSURF_DIR: " + output);
        Check(parts.Length == 3 && parts[2].StartsWith("twllink "), "twllink runs from Neovim's shell (:!): " + output);
        Console.WriteLine("neovim: " + output.Replace("\r", "").Replace("\n", " / "));
    }

    static void Pump(TimeSpan time)
    {
        var clock = Stopwatch.StartNew();
        while (clock.Elapsed < time)
        {
            Dispatcher.UIThread.RunJobs();
            System.Threading.Thread.Sleep(10);
        }
    }

    static void Await(Task task)
    {
        var clock = Stopwatch.StartNew();
        while (!task.IsCompleted && clock.Elapsed < TimeSpan.FromSeconds(30))
        {
            Dispatcher.UIThread.RunJobs();
            System.Threading.Thread.Sleep(10);
        }
        Dispatcher.UIThread.RunJobs();
        if (task.IsFaulted)
            throw task.Exception!;
    }

    static void ShootBoth(string outDir, string name, ScriptDevViewModel vm)
    {
        foreach (var theme in new[] { ThemeVariant.Light, ThemeVariant.Dark })
        {
            Application.Current!.RequestedThemeVariant = theme;
            var w = new Window { Width = 938, Height = 665, Content = new TweakerUI.Views.ScriptDevView { DataContext = vm } };
            w.Show();
            for (var i = 0; i < 5; i++)
                Dispatcher.UIThread.RunJobs();
            using (var frame = w.CaptureRenderedFrame())
                frame!.Save(Path.Combine(outDir, $"{theme}-{name}.png"));
            w.Close();
        }
    }
}
