using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using Avalonia;
using Avalonia.Animation;
using Avalonia.Controls;
using Avalonia.Controls.Presenters;
using Avalonia.Controls.Primitives;
using Avalonia.Headless;
using Avalonia.Input;
using Avalonia.Markup.Xaml;
using Avalonia.Markup.Xaml.Styling;
using Avalonia.Media;
using Avalonia.Styling;
using Avalonia.Themes.Fluent;
using Avalonia.Threading;
using Avalonia.VisualTree;
using SkiaSharp;

// Mirrors App.axaml: same dictionaries, same style files, same order. Keep in step with it.
class App : Application
{
    public override void Initialize()
    {
        foreach (var d in new[] { "NeutralColors", "AccentColors", "SharedControlTemplates" })
            Resources.MergedDictionaries.Add(new ResourceInclude(new Uri("avares://TweakerUI/")) { Source = new Uri($"avares://TweakerUI/Themes/{d}.axaml") });
        Styles.Add(new FluentTheme());
        foreach (var s in new[] { "ScrollBarStyles", "SharedStyles", "Styles", "SkinChangerStyles", "TweakerStyles", "SettingsStyles", "StatusBarStyles", "QuickPlayerStyles", "ScriptsStyles" })
            Styles.Add(new StyleInclude(new Uri("avares://TweakerUI/")) { Source = new Uri($"avares://TweakerUI/Themes/{s}.axaml") });
    }
}

static class Program
{
    [STAThread]
    static int Main(string[] args)
    {
        if (args.Length == 0)
        {
            Console.WriteLine("modes: shoot <dir> | compare <dirA> <dirB> | diffmap <a.png> <b.png> <out.png> | transitions | colors <dir> | mem [fast] [rounds=N] [cycles=N] | tabs | scripts <dir> | scriptdev <dir> | readme <dir>");
            return 2;
        }

        switch (args[0])
        {
            case "compare": return Snapshots.Compare(args[1], args[2]);
            case "diffmap": Snapshots.DiffMap(args[1], args[2], args[3]); return 0;
        }

        AppBuilder.Configure<App>().UseSkia().WithInterFont()
            .UseHeadless(new AvaloniaHeadlessPlatformOptions { UseHeadlessDrawing = false }) // real Skia text metrics
            .SetupWithoutStarting();

        switch (args[0])
        {
            case "shoot": Snapshots.Shoot(args[1]); return 0;
            case "transitions": Transitions.Sweep(); return 0;
            case "colors": Snapshots.ColorsTab(args[1]); return 0;
            case "mem": Memory.Run(args); return 0;
            case "tabs": TabLeak.Run(); return 0;
            case "scripts": return ScriptsTab.Run(args[1]);
            case "scriptdev": return ScriptDevTab.Run(args[1]);
            case "readme": return ReadmeShots.Run(args[1]);
            default: Console.WriteLine($"unknown mode {args[0]}"); return 2;
        }
    }
}

// Pixel snapshots: every tab (empty DataContext - layout and styles, not data) plus a gallery of the shared
// building blocks in every interactive state, both themes. shoot before a change, shoot after, compare.
static class Snapshots
{
    // id, element xaml ({C} = classes), states
    static readonly (string id, string xaml, string[] states)[] Gallery =
    {
        ("card", "<Border Classes='Card' HorizontalAlignment='Left'><TextBlock Text='Card body'/></Border>", new[] { "rest" }),
        ("card-roomy", "<Border Classes='Card Roomy' HorizontalAlignment='Left'><TextBlock Text='Card body'/></Border>", new[] { "rest" }),
        ("card-uniform", "<Border Classes='Card Uniform' HorizontalAlignment='Left'><TextBlock Text='Card body'/></Border>", new[] { "rest" }),
        ("card-tinted", "<Border Classes='Card Tinted' HorizontalAlignment='Left'><TextBlock Text='Card body'/></Border>", new[] { "rest" }),
        ("card-qp-character", "<Border Classes='Card Tinted QPCharacterPanel' HorizontalAlignment='Left'><TextBlock Text='Card body'/></Border>", new[] { "rest" }),
        ("switch", "<ToggleButton Classes='Switch'/>", new[] { "rest", "pointerover", "checked" }),
        ("toggle-row", "<Border Classes='ToggleRow' Width='260'><TextBlock Classes='ToggleLabel' Text='Toggle row'/></Border>", new[] { "rest", "pointerover", "disabled" }),
        ("toggle-row-nested", "<Border Classes='ToggleRow STNested' Width='260'><TextBlock Classes='ToggleLabel' Text='Toggle row'/></Border>", new[] { "rest", "pointerover", "disabled" }),
        ("card-title", "<TextBlock Classes='CardTitle' Text='Card title'/>", new[] { "rest" }),
        ("empty-state", "<Grid Width='300' Height='60'><TextBlock Classes='EmptyState' Text='Nothing here yet'/></Grid>", new[] { "rest" }),
        ("icon", "<Button Classes='IconButton'><Path Data='M0 0L10 10M10 0L0 10' Stroke='Gray' StrokeThickness='2' Width='12' Height='12'/></Button>", new[] { "rest", "pointerover", "disabled" }),
        ("icon-danger", "<Button Classes='IconButton Danger'><Path Data='M0 0L10 10M10 0L0 10' Stroke='Gray' StrokeThickness='2' Width='12' Height='12'/></Button>", new[] { "rest", "pointerover" }),
        ("icon-sc-rail", "<Button Classes='IconButton SCRailButton'><Path Data='M0 0L10 10M10 0L0 10' Stroke='Gray' StrokeThickness='2' Width='12' Height='12'/></Button>", new[] { "rest", "pointerover" }),
        ("icon-qp", "<Button Classes='IconButton QPIconButton'><Path Data='M0 0L10 10M10 0L0 10' Stroke='Gray' StrokeThickness='2' Width='12' Height='12'/></Button>", new[] { "rest", "pointerover" }),
        ("icon-qp-row", "<Button Classes='IconButton QPRowIconButton'><Path Data='M0 0L10 10M10 0L0 10' Stroke='Gray' StrokeThickness='2' Width='12' Height='12'/></Button>", new[] { "rest", "pointerover" }),
        ("list-sc", "<ListBox Classes='RowList' Width='260'><ListBoxItem Name='t' Height='40'>Item</ListBoxItem><ListBoxItem>Other</ListBoxItem></ListBox>", new[] { "rest", "pointerover", "selected" }),
        ("list-qp-playlist", "<ListBox Classes='QPPlaylistList' Width='260'><ListBoxItem Name='t' Height='40'>Item</ListBoxItem><ListBoxItem>Other</ListBoxItem></ListBox>", new[] { "rest", "pointerover", "selected" }),
        ("list-qp-queue", "<ListBox Classes='QPQueueList' Width='260'><ListBoxItem Name='t' Height='40'>Item</ListBoxItem><ListBoxItem>Other</ListBoxItem></ListBox>", new[] { "rest", "pointerover", "selected" }),
    };

    static readonly string[] Views = { "SkinChangerView", "ColorsConfiguratorView", "TweakerView", "QuickPlayerView", "ScriptsView", "ScriptDevView", "SettingView" };

    // Headless animation clocks don't advance reliably (a transition can sit at its first frame forever),
    // and Avalonia 12 keeps IClock/ClockBase internal, so there is no manual clock to drive instead. Dropping
    // every Transitions collection makes a state change land on its final value at once - which is what a
    // snapshot wants. Transitions themselves are measured by math in Transitions.Sweep.
    static void DropTransitions(Visual root)
    {
        foreach (var v in new[] { root }.Concat(root.GetVisualDescendants()))
            if (v is Animatable a)
                a.Transitions = null;
    }

    static void Settle()
    {
        for (int i = 0; i < 5; i++)
            Dispatcher.UIThread.RunJobs();
    }

    static void Save(Window w, string file)
    {
        using var frame = w.CaptureRenderedFrame();
        frame.Save(file);
    }

    public static void Shoot(string outDir)
    {
        Directory.CreateDirectory(outDir);
        foreach (var theme in new[] { ThemeVariant.Light, ThemeVariant.Dark })
        {
            Application.Current.RequestedThemeVariant = theme;
            foreach (var (id, xaml, states) in Gallery)
            {
                foreach (var state in states)
                {
                    var w = (Window)AvaloniaRuntimeXamlLoader.Load(
                        "<Window xmlns='https://github.com/avaloniaui' Width='360' Height='140' Background='{DynamicResource SurfaceBrush}'>"
                        + "<StackPanel Margin='20' HorizontalAlignment='Left' VerticalAlignment='Top'>" + xaml + "</StackPanel></Window>");
                    w.Show();
                    Settle();
                    DropTransitions(w);
                    var target = w.GetVisualDescendants().OfType<Control>().FirstOrDefault(c => c.Name == "t")
                                 ?? ((StackPanel)w.Content).Children[0];
                    w.MouseMove(new Point(359, 139));
                    switch (state)
                    {
                        case "pointerover":
                            w.MouseMove(target.TranslatePoint(new Point(target.Bounds.Width / 2, target.Bounds.Height / 2), w).Value);
                            break;
                        case "checked": ((ToggleButton)target).IsChecked = true; break;
                        case "selected": ((ListBoxItem)target).IsSelected = true; break;
                        case "disabled": target.IsEnabled = false; break;
                    }
                    Settle();
                    Save(w, Path.Combine(outDir, $"{theme}-{id}-{state}.png"));
                    w.Close();
                }
            }

            foreach (var view in Views)
            {
                var type = typeof(TweakerUI.App).Assembly.GetType("TweakerUI.Views." + view);
                var w = new Window { Width = 1150, Height = 780, Content = Activator.CreateInstance(type) };
                w.Show();
                Settle();
                DropTransitions(w);
                Settle();
                Save(w, Path.Combine(outDir, $"{theme}-view-{view}.png"));
                w.Close();
            }
        }
        Console.WriteLine($"{Directory.GetFiles(outDir, "*.png").Length} snapshots in {outDir}");
    }

    // Colors Configurator with its REAL view model, at the module width of a minimum (1000px) and a
    // 1920px window. The view model reads/writes palettes.pltc next to the running exe (AppPaths) - run
    // the stand through UiStand.exe, not "dotnet UiStand.dll", or ProcessPath is dotnet.exe itself.
    public static void ColorsTab(string outDir)
    {
        Directory.CreateDirectory(outDir);
        foreach (var width in new[] { 738, 1658 })
        {
            var view = new TweakerUI.Views.ColorsConfiguratorView { DataContext = new TweakerUI.ViewModels.ColorsConfiguratorViewModel() };
            var w = new Window { Width = width, Height = 665, Content = view };
            w.Show();
            Settle();
            Save(w, Path.Combine(outDir, $"colors-{width}.png"));
            w.Close();
        }
    }

    public static int Compare(string a, string b)
    {
        int same = 0;
        foreach (var fa in Directory.GetFiles(a, "*.png").OrderBy(f => f))
        {
            var fb = Path.Combine(b, Path.GetFileName(fa));
            if (!File.Exists(fb)) { Console.WriteLine($"MISSING  {Path.GetFileName(fa)}"); continue; }
            using var ba = SKBitmap.Decode(fa);
            using var bb = SKBitmap.Decode(fb);
            if (ba.Width != bb.Width || ba.Height != bb.Height) { Console.WriteLine($"SIZE     {Path.GetFileName(fa)}"); continue; }
            int diff = 0, maxDelta = 0;
            for (int y = 0; y < ba.Height; y++)
            for (int x = 0; x < ba.Width; x++)
            {
                var p = ba.GetPixel(x, y);
                var q = bb.GetPixel(x, y);
                var d = Math.Max(Math.Max(Math.Abs(p.Red - q.Red), Math.Abs(p.Green - q.Green)), Math.Max(Math.Abs(p.Blue - q.Blue), Math.Abs(p.Alpha - q.Alpha)));
                if (d > 0) { diff++; maxDelta = Math.Max(maxDelta, d); }
            }
            if (diff == 0) same++;
            else Console.WriteLine($"DIFF     {Path.GetFileName(fa),-50} {diff,7} px, max channel delta {maxDelta}");
        }
        Console.WriteLine($"identical: {same}");
        return 0;
    }

    // Copy of b with every pixel that differs from a painted red, plus the bounding box of the change.
    public static void DiffMap(string a, string b, string outFile)
    {
        using var ba = SKBitmap.Decode(a);
        using var bb = SKBitmap.Decode(b);
        using var vis = bb.Copy();
        int x0 = int.MaxValue, y0 = int.MaxValue, x1 = -1, y1 = -1;
        for (int y = 0; y < ba.Height; y++)
        for (int x = 0; x < ba.Width; x++)
            if (ba.GetPixel(x, y) != bb.GetPixel(x, y))
            {
                x0 = Math.Min(x0, x); y0 = Math.Min(y0, y); x1 = Math.Max(x1, x); y1 = Math.Max(y1, y);
                vis.SetPixel(x, y, SKColors.Red);
            }
        Console.WriteLine($"diff bbox x {x0}..{x1}, y {y0}..{y1}");
        using var img = SKImage.FromBitmap(vis);
        using var data = img.Encode(SKEncodedImageFormat.Png, 100);
        File.WriteAllBytes(outFile, data.ToArray());
    }
}

// Every animated brush change of every interactive class, both themes: how far the ON-SCREEN color strays
// from a straight crossfade, for Avalonia's BrushTransition math and for CrossfadeBrushTransition
// (avalonia-pitfalls.md §5). Endpoints are the BASE values (GetBaseValue - no animation layer) of every
// template part before and after the state change; frames are computed, not rendered, which sidesteps
// the headless clock problem entirely.
static class Transitions
{
    static readonly MethodInfo BuiltIn = typeof(Animatable).Assembly.GetType("Avalonia.Animation.Animators.ColorAnimator")
        .GetMethod("InterpolateCore", BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.Public);
    static readonly object Crossfade = new TweakerUI.Core.CrossfadeBrushTransition();
    static readonly MethodInfo CrossfadeInterp = Crossfade.GetType().GetMethod("Interpolate", BindingFlags.Instance | BindingFlags.NonPublic);

    static Color? ColorOf(IBrush b) => b switch
    {
        null => Colors.Transparent,
        ISolidColorBrush s => Color.FromArgb((byte)Math.Round(s.Color.A * s.Opacity), s.Color.R, s.Color.G, s.Color.B),
        _ => null
    };

    static (double r, double g, double b) Over(Color c, Color bg)
    {
        double a = c.A / 255.0;
        return (c.R * a + bg.R * (1 - a), c.G * a + bg.G * (1 - a), c.B * a + bg.B * (1 - a));
    }

    // Largest distance (0..255) of any on-screen frame from the line between the on-screen endpoints.
    static double Hump(List<Color> frames, Color bg)
    {
        var p = frames.Select(c => Over(c, bg)).ToList();
        var s = p[0];
        var e = p[^1];
        double dx = e.r - s.r, dy = e.g - s.g, dz = e.b - s.b;
        double len2 = dx * dx + dy * dy + dz * dz;
        double worst = 0;
        foreach (var q in p)
        {
            double t = len2 == 0 ? 0 : Math.Clamp(((q.r - s.r) * dx + (q.g - s.g) * dy + (q.b - s.b) * dz) / len2, 0, 1);
            double cx = s.r + t * dx - q.r, cy = s.g + t * dy - q.g, cz = s.b + t * dz - q.b;
            worst = Math.Max(worst, Math.Sqrt(cx * cx + cy * cy + cz * cz));
        }
        return worst;
    }

    static readonly AvaloniaProperty[] BrushProps =
    {
        Border.BackgroundProperty, Border.BorderBrushProperty, TemplatedControl.BackgroundProperty, TemplatedControl.BorderBrushProperty,
        TemplatedControl.ForegroundProperty, ContentPresenter.BackgroundProperty, ContentPresenter.BorderBrushProperty,
        ContentPresenter.ForegroundProperty, Avalonia.Controls.Shapes.Shape.FillProperty, Avalonia.Controls.Shapes.Shape.StrokeProperty, TextBlock.ForegroundProperty,
    };

    static Dictionary<(int, string), (IBrush brush, bool hasTransition, string where)> Snapshot(Control root)
    {
        var result = new Dictionary<(int, string), (IBrush, bool, string)>();
        var visuals = new[] { (Visual)root }.Concat(root.GetVisualDescendants()).ToList();
        for (int i = 0; i < visuals.Count; i++)
        {
            if (visuals[i] is not Animatable a)
                continue;
            foreach (var p in BrushProps)
            {
                if (!AvaloniaPropertyRegistry.Instance.IsRegistered(visuals[i], p))
                    continue;
                var brush = ((AvaloniaObject)visuals[i]).GetBaseValue(p) as IBrush;
                var hasTransition = a.Transitions?.Any(t => (t as TransitionBase)?.Property == p) == true;
                var name = visuals[i] is StyledElement se && se.Name != null ? $"{visuals[i].GetType().Name}#{se.Name}" : visuals[i].GetType().Name;
                result[(i, p.Name)] = (brush, hasTransition, name);
            }
        }
        return result;
    }

    public static void Sweep()
    {
        var buttons = new[]
        {
            "CCDiscard", "CCGhost", "CCGradientButton", "CCNegative", "CCNewPalette", "CCPaletteRow", "CCPaletteRow Selected", "CCSecondary",
            "CCTab", "IconResetButton", "QPCharacterHeader", "QPTrackRemoveButton", "QPTransportButton", "SCSegment", "ActionButton", "ActionButton STBrowseButton",
            "TWCommandButton", "TitleBarButton", "TitleBarButton TitleBarButtonClose", "UnderscoredButton",
            "IconButton", "IconButton Danger", "IconButton SCRailButton", "IconButton QPIconButton", "IconButton QPRowIconButton",
        };
        var cases = new List<(string name, string xaml, string[] states)>();
        foreach (var b in buttons)
            cases.Add(($"Button.{b.Replace(' ', '.')}", $"<Button Name='t' Classes='{b}' Content='Label' Width='160' Height='36'/>", new[] { "pointerover", "pressed" }));
        cases.Add(("ToggleButton.Switch", "<ToggleButton Name='t' Classes='Switch'/>", new[] { "pointerover", "checked" }));
        foreach (var b in new[] { "NavItem", "QPCharacterButton", "QPSegmentButton", "QPSegmentIconButton" })
            cases.Add(($"RadioButton.{b}", $"<RadioButton Name='t' Classes='{b}' Content='Label' Width='160' Height='36'/>", new[] { "pointerover", "checked" }));
        foreach (var l in new[] { "QPPlaylistList", "QPQueueList", "RowList" })
            cases.Add(($"ListBox.{l} item", $"<ListBox Classes='{l}' Width='250'><ListBoxItem Name='t' Height='40'>Item</ListBoxItem><ListBoxItem>Other</ListBoxItem></ListBox>", new[] { "pointerover", "selected" }));
        cases.Add(("Border.ToggleRow", "<Border Name='t' Classes='ToggleRow' Width='250' Height='40'><TextBlock Text='Row'/></Border>", new[] { "pointerover" }));

        foreach (var theme in new[] { ThemeVariant.Light, ThemeVariant.Dark })
        {
            Application.Current.RequestedThemeVariant = theme;
            int total = 0, brokenBuiltIn = 0, brokenCrossfade = 0;
            var lines = new List<string>();
            foreach (var (name, xaml, states) in cases)
            {
                foreach (var state in states)
                {
                    var w = (Window)AvaloniaRuntimeXamlLoader.Load(
                        "<Window xmlns='https://github.com/avaloniaui' Width='600' Height='300' Background='{DynamicResource SurfaceBrush}'><StackPanel Margin='20'>"
                        + xaml + "</StackPanel></Window>");
                    w.Show();
                    Dispatcher.UIThread.RunJobs();
                    var bg = ColorOf((IBrush)w.FindResource(theme, "SurfaceBrush")).Value;
                    var t = w.GetVisualDescendants().OfType<Control>().First(c => c.Name == "t");
                    var p = t.TranslatePoint(new Point(t.Bounds.Width / 2, t.Bounds.Height / 2), w).Value;
                    w.MouseMove(new Point(599, 299));
                    Dispatcher.UIThread.RunJobs();
                    if (state != "pointerover")
                    {
                        w.MouseMove(p); // "from" is the hovered state for pressed/checked/selected
                        Dispatcher.UIThread.RunJobs();
                    }
                    var before = Snapshot(t);
                    switch (state)
                    {
                        case "pointerover": w.MouseMove(p); break;
                        case "pressed": w.MouseDown(p, MouseButton.Left); break;
                        case "checked": ((ToggleButton)t).IsChecked = true; break;
                        case "selected": ((ListBoxItem)t).IsSelected = true; break;
                    }
                    Dispatcher.UIThread.RunJobs();
                    var after = Snapshot(t);

                    foreach (var (key, (b0, tr, where)) in before)
                    {
                        if (!after.TryGetValue(key, out var a1) || !tr)
                            continue;
                        var c0 = ColorOf(b0);
                        var c1 = ColorOf(a1.brush);
                        if (c0 == null || c1 == null || c0 == c1)
                            continue;
                        total++;
                        var hb = Hump(Enumerable.Range(0, 21).Select(i => (Color)BuiltIn.Invoke(null, new object[] { i / 20.0, c0.Value, c1.Value })).ToList(), bg);
                        var hc = Hump(Enumerable.Range(0, 21).Select(i => ColorOf((IBrush)CrossfadeInterp.Invoke(Crossfade, new object[] { i / 20.0, b0, a1.brush })).Value).ToList(), bg);
                        if (hb > 3) brokenBuiltIn++;
                        if (hc > 3) brokenCrossfade++;
                        if (hb > 3 || hc > 3)
                            lines.Add($"  {name,-34} :{state,-11} {where}.{key.Item2,-11} {c0} -> {c1}   built-in {hb,5:F1}   crossfade {hc,4:F1}");
                    }
                    w.Close();
                }
            }
            Console.WriteLine($"== {theme}: animated color changes {total}; visibly off (>3) with BrushTransition {brokenBuiltIn}, with CrossfadeBrushTransition {brokenCrossfade}");
            lines.ForEach(Console.WriteLine);
        }
    }
}
