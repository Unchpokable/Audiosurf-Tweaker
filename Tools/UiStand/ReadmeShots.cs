using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using AudiosurfInterface;
using Avalonia;
using Avalonia.Animation;
using Avalonia.Controls;
using Avalonia.Headless;
using Avalonia.Styling;
using Avalonia.Threading;
using Avalonia.VisualTree;
using QuickPlayerCore;
using QuickPlayerCore.Audiosurf;
using SkiaSharp;
using TweakerCore.PluginInstall;
using TweakerUI.Core;
using TweakerUI.ViewModels;
using Application = Avalonia.Application;

// Screenshots for the public README: the REAL MainWindow with the REAL view models, staged the way the app looks
// in use - a fake game folder with the plugin installed from the real PluginPayload\, the skins from Skins\ next to
// UiStand.exe, a demo playlist of made-up tracks with generated covers, the real puzzlepro_hud project built by the
// real twllink, and the game "connected" (the scripts' live states fed through ScriptsOverlayBridge's fake transport).
//
// Nothing here talks to a real game: MainWindowViewModel is built without its constructor (that one detects the real
// install and, on Opened, would sync the plugin into it), and the window's Opened handler is detached for the same
// reason. Paths the tabs print are replaced with the default Steam location, so no temp folder ends up in a picture.
static class ReadmeShots
{
    const string ShownGame = @"C:\Program Files (x86)\Steam\steamapps\common\Audiosurf";

    static readonly Type Settings = typeof(SkinChangerViewModel).Assembly.GetType("TweakerUI.Core.SettingsProvider");

    static readonly (string file, string nav, Func<Shell, ViewModelBase> vm)[] Tabs =
    {
        ("skin-changer", "Skin Changer", s => s.Skins),
        ("colors", "Colors Configurator", s => s.Colors),
        ("command-and-control", "Command and Control", s => s.Tweaker),
        ("quick-player", "Quick Player", s => s.Player),
        ("scripts", "Scripts", s => s.Scripts),
        ("script-dev", "Script Dev", s => s.ScriptDev),
        ("settings", "Settings", s => s.Settings),
    };

    sealed class Shell
    {
        public SkinChangerViewModel Skins;
        public ColorsConfiguratorViewModel Colors;
        public TweakerViewModel Tweaker;
        public QuickPlayerViewModel Player;
        public ScriptsViewModel Scripts;
        public ScriptDevViewModel ScriptDev;
        public SettingViewModel Settings;
    }

    public static int Run(string outDir)
    {
        Directory.CreateDirectory(outDir);
        var exeDir = Path.GetDirectoryName(Environment.ProcessPath)!;
        var uiBin = Path.GetFullPath(Path.Combine(exeDir, "..", "..", "..", "..", "..", "..", "TweakerUI", "bin", "x64", "Debug", "net10.0-windows10.0.19041.0"));
        var repo = Path.GetFullPath(Path.Combine(exeDir, "..", "..", "..", "..", "..", ".."));
        var root = Path.Combine(Path.GetTempPath(), "UiStand-readme-" + Guid.NewGuid().ToString("N"));
        var engine = Path.Combine(root, "Audiosurf", "engine");
        var payload = Path.Combine(exeDir, "PluginPayload");
        var devTools = Path.Combine(exeDir, "DevTools");
        var quickPlayer = Path.Combine(exeDir, "QuickPlayer");
        var quickPlayerAside = quickPlayer + ".readme-aside";
        var palettes = ColorsConfiguratorViewModel.PaletteContainerFilename + ".pltc";
        var palettesAside = palettes + ".readme-aside";

        if (!Directory.Exists(Path.Combine(exeDir, "Skins")) || !Directory.EnumerateFiles(Path.Combine(exeDir, "Skins")).Any())
        {
            Console.WriteLine("readme: put some skins into Skins\\ next to UiStand.exe first");
            return 1;
        }

        try
        {
            // --- the game, with the plugin installed the way PluginService installs it ---
            CopyTree(Path.Combine(uiBin, "PluginPayload"), payload);
            Directory.CreateDirectory(Path.Combine(engine, "channels"));
            Directory.CreateDirectory(Path.Combine(engine, "textures"));
            File.WriteAllText(Path.Combine(engine, "QuestViewer.exe"), "not really the game");
            var report = PluginInstallation.Install(engine, payload);
            if (report.Errors.Count > 0)
                Console.WriteLine("readme: install reported " + string.Join("; ", report.Errors.Select(e => e.Message)));
            SetSetting("GameTexturesPath", Path.Combine(engine, "textures"));

            // --- Quick Player's playlists, set aside and put back afterwards ---
            if (Directory.Exists(quickPlayer))
                Directory.Move(quickPlayer, quickPlayerAside);
            SeedPlaylists(Path.Combine(root, "Music"));

            // --- Colors Configurator's palettes, the same ---
            if (File.Exists(palettes))
                File.Move(palettes, palettesAside, true);
            SeedPalettes(palettes);

            // --- Script Dev: twllink next to the exe, as in the app ---
            if (Directory.Exists(devTools))
                Directory.Delete(devTools, true);
            CopyTree(Path.Combine(uiBin, "DevTools", "Binaries"), Path.Combine(devTools, "Binaries"));

            var shell = Build(root, engine, repo);

            var window = new TweakerUI.Views.MainWindow();
            var onOpened = typeof(TweakerUI.Views.MainWindow).GetMethod("OnOpened", BindingFlags.Instance | BindingFlags.NonPublic, new[] { typeof(object), typeof(EventArgs) })!;
            window.Opened -= (EventHandler)Delegate.CreateDelegate(typeof(EventHandler), window, onOpened);
            window.DataTemplates.Add(new TweakerUI.ViewLocator());
            window.DataContext = ShellViewModel(shell);
            window.Width = 1200;
            window.Height = 720;
            window.Show();
            Settle();

            foreach (var theme in new[] { ThemeVariant.Dark, ThemeVariant.Light })
            {
                Application.Current!.RequestedThemeVariant = theme;
                foreach (var (file, nav, vm) in Tabs)
                {
                    var button = window.GetVisualDescendants().OfType<RadioButton>().Single(r => (r.Content as string) == nav);
                    button.IsChecked = true;
                    Restage(shell, theme);
                    ((MainWindowViewModel)window.DataContext).CurrentView = vm(shell);
                    Settle();
                    DropTransitions(window);
                    Pump(300);
                    Arrange(window, file, shell);
                    Settle();
                    Scrub(window, root);
                    Settle();
                    using (var frame = window.CaptureRenderedFrame())
                        frame!.Save(Path.Combine(outDir, $"{file}-{theme.ToString().ToLowerInvariant()}.png"));
                }
            }

            window.Close();
            shell.Scripts.SetActive(false);
            shell.ScriptDev.SetActive(false);
        }
        finally
        {
            ScriptsOverlayBridge.UseTransport(_ => false, () => false);
            TryDelete(root);
            TryDelete(payload);
            TryDelete(devTools);
            TryDelete(quickPlayer);
            if (Directory.Exists(quickPlayerAside))
                Directory.Move(quickPlayerAside, quickPlayer);
            if (File.Exists(palettesAside))
                File.Move(palettesAside, palettes, true);
            else if (File.Exists(palettes))
                File.Delete(palettes);
        }

        Console.WriteLine($"readme: {Directory.GetFiles(outDir, "*.png").Length} screenshots in {outDir}");
        return 0;
    }

    // --- the tabs, staged -------------------------------------------------------------------------------------

    static Shell Build(string root, string engine, string repo)
    {
        var shell = new Shell();

        // Skin Changer: the list read from Skins\, one skin selected with its screenshots, and it is the installed one.
        shell.Skins = new SkinChangerViewModel();
        Pump(() => shell.Skins.Skins.Count > 0 && !shell.Skins.LoadingVisible, 60000);
        var pick = shell.Skins.Skins.FirstOrDefault(c => c.Name.StartsWith("Glitchwave")) ?? shell.Skins.Skins[0];
        shell.Skins.SelectedItem = pick;
        Pump(() => !shell.Skins.IsLoadingScreenshots, 60000);
        Pump(300);
        shell.Skins.CurrentInstalledSkinRaw = pick.Name;

        shell.Colors = new ColorsConfiguratorViewModel();
        typeof(ColorsConfiguratorViewModel).GetMethod("SelectPalette", BindingFlags.Instance | BindingFlags.NonPublic)!
            .Invoke(shell.Colors, new object[] { shell.Colors.Palettes.Single(p => p.Name == "Cyberpunk") });

        // Command and Control: the game is there, a couple of tweaks are on.
        shell.Tweaker = new TweakerViewModel();
        Set(shell.Tweaker, "isAudiosurfConnected", true, "IsAudiosurfConnected");
        Set(shell.Tweaker, "hiddenSongTweakActive", true, "HiddenSongTweakActive");
        Set(shell.Tweaker, "bankingCameraTweakActive", true, "BankingCameraTweakActive");

        // A few lines in the log, the way the console prints what went to the game and what came back.
        var console = typeof(TweakerViewModel).GetField("_console", BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(shell.Tweaker)!;
        void Log(string method, object argument) =>
            console.GetType().GetMethod(method, BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(console, new[] { null, argument });
        Log("AppendInfo", new CommandInfo(GameProtocol.Command(GameProtocol.ReloadTextures), CommandInfo.CommandStatus.Sent));
        Log("AppendInfo", new CommandInfo(GameProtocol.Command(GameProtocol.GoToCharacterScreen), CommandInfo.CommandStatus.Sent));
        Log("AppendIncomingMessage", "oncharacterscreen");

        shell.Player = new QuickPlayerViewModel();
        shell.Player.SelectedPlaylistRow = shell.Player.Playlists.Single(p => p.Playlist.Name == "Night Drive");
        Pump(2000); // covers and durations load in the background

        // Script Dev: the shipped PuzzlePRO HUD project (17 modules) and a fresh one, the first one built.
        shell.ScriptDev = new ScriptDevViewModel();
        shell.ScriptDev.SetActive(true);
        var projectsRoot = Path.Combine(root, "Projects");
        var hud = Path.Combine(projectsRoot, "puzzlepro_hud");
        CopyTree(Path.Combine(repo, "TweakerPlugin", "assets", "script-projects", "puzzlepro_hud"), hud);
        TryDelete(Path.Combine(hud, "build"));
        shell.ScriptDev.StartCreateCommand.Execute(null);
        shell.ScriptDev.NewName = "Lap Timer";
        shell.ScriptDev.NewDescription = "Splits every song into quarters and shows how each one went against your best.";
        shell.ScriptDev.NewTemplate = shell.ScriptDev.Templates.Single(t => t.Key == "hud");
        Await(shell.ScriptDev.CreateCommand.ExecuteAsync(null));
        // The template signs the project with the account name of whoever runs the stand.
        var manifest = Path.Combine(shell.ScriptDev.Selected.Root, "twproject.json");
        File.WriteAllText(manifest, File.ReadAllText(manifest).Replace($"\"{Environment.UserName}\"", "\"Unchpokable\""));
        shell.ScriptDev.RefreshCommand.Execute(null);
        Await(shell.ScriptDev.InstallCommand.ExecuteAsync(null));
        var projects = typeof(ScriptDevViewModel).GetField("_projects", BindingFlags.Instance | BindingFlags.NonPublic)!.GetValue(shell.ScriptDev)!;
        projects.GetType().GetMethod("Remember")!.Invoke(projects, new object[] { hud });
        shell.ScriptDev.RefreshCommand.Execute(null);
        shell.ScriptDev.Selected = shell.ScriptDev.Projects.Single(p => p.Project.Id == "puzzlepro_hud");
        Await(shell.ScriptDev.BuildCommand.ExecuteAsync(null));
        if (!shell.ScriptDev.Selected.Succeeded)
            Console.WriteLine("readme: puzzlepro_hud did not build: " + shell.ScriptDev.Selected.BuildStatus + " / " + shell.ScriptDev.Selected.BuildDetail);

        // Scripts: the game is running and the plugin has reported every script.
        var sent = new System.Collections.Generic.List<string>();
        ScriptsOverlayBridge.UseTransport(line => { sent.Add(line); return true; }, () => true);
        var installed = Directory.GetFiles(PluginInstallation.ScriptsPath(engine), "*.lua").Select(Path.GetFileName).OrderBy(f => f).ToList();
        foreach (var file in installed)
            ScriptsOverlayBridge.Receive($"SCRIPT_STATE {file} true running");
        ScriptsOverlayBridge.Receive($"SCRIPT_SYNCED {installed.Count}");
        shell.Scripts = new ScriptsViewModel();
        shell.Scripts.SetActive(true);
        Pump(() => shell.Scripts.Summary != null || shell.Scripts.EmptyText != null, 5000);
        shell.Scripts.GameRunning = true;
        ScriptsOverlayBridge.Receive($"SCRIPT_STATE {installed[0]} true running"); // re-lay the rows with the game up
        Pump(200);

        // The overlay linked to Tweaker, as the plugin's handshake leaves it.
        SetSetting("SyncOverlayWithTweaker", true);
        var overlay = typeof(SkinChangerViewModel).Assembly.GetType("TweakerUI.Core.OverlayHelper")!;
        var linkState = overlay.Assembly.GetType("TweakerUI.Core.PluginLinkState")!;
        overlay.GetField("_linkState", BindingFlags.Static | BindingFlags.NonPublic)!.SetValue(null, Enum.Parse(linkState, "Connected"));
        overlay.GetField("_connectedVersion", BindingFlags.Static | BindingFlags.NonPublic)!.SetValue(null, PluginInstallation.InstalledVersion(engine));
        overlay.GetField("_connectedLoadMode", BindingFlags.Static | BindingFlags.NonPublic)!.SetValue(null, "channels");

        shell.Settings = new SettingViewModel();
        Set(shell.Settings, "texturesFolderPath", ShownGame + @"\engine\textures", "TexturesFolderPath");

        return shell;
    }

    static MainWindowViewModel ShellViewModel(Shell shell)
    {
        // The constructor detects the real game and wires the app's services to it - everything it would build is
        // already here, so the object is assembled field by field instead.
        var vm = (MainWindowViewModel)RuntimeHelpers.GetUninitializedObject(typeof(MainWindowViewModel));
        void Backing(string property, object value) =>
            typeof(MainWindowViewModel).GetField($"<{property}>k__BackingField", BindingFlags.Instance | BindingFlags.NonPublic)!.SetValue(vm, value);
        Backing("SkinChangerVM", shell.Skins);
        Backing("ColorsVM", shell.Colors);
        Backing("TweakerVM", shell.Tweaker);
        Backing("QuickPlayerVM", shell.Player);
        Backing("ScriptsVM", shell.Scripts);
        Backing("ScriptDevVM", shell.ScriptDev);
        Backing("SettingsVM", shell.Settings);
        typeof(MainWindowViewModel).GetField("currentView", BindingFlags.Instance | BindingFlags.NonPublic)!.SetValue(vm, shell.Skins);

        var handle = AudiosurfHandle.Instance;
        typeof(AudiosurfHandle).GetField("_currentState", BindingFlags.Instance | BindingFlags.NonPublic)!.SetValue(handle, ASHandleState.Connected);
        typeof(MainWindowViewModel).GetField("_asHandle", BindingFlags.Instance | BindingFlags.NonPublic)!.SetValue(vm, handle);
        return vm;
    }

    // --- Quick Player's demo playlists ------------------------------------------------------------------------

    static void SeedPlaylists(string music)
    {
        Directory.CreateDirectory(music);

        // Made-up artists and titles; the covers are generated below, so no one's library ends up in the picture.
        // Fixed ids: the list is read in file-name order, and Night Drive goes first.
        var night = new Playlist { Id = new Guid("00000000-0000-0000-0000-000000000001"), Name = "Night Drive", Mode = PlaybackMode.ShuffleLoop };
        (string artist, string title, int seconds, bool flac, uint a, uint b, GameCharacter? character, SongTagToken[] tags)[] tracks =
        {
            ("Neon Harbor", "Afterglow Freeway", 221, true, 0xFF2B1055, 0xFFD53A9D, null, new SongTagToken[0]),
            ("Lumen Drift", "Static Bloom", 254, false, 0xFF0F2027, 0xFF2C9AA8, GameCharacter.PointmanElite, new[] { SongTagToken.Steep }),
            ("Kite Engine", "Satellite Hearts", 198, false, 0xFF3A1C71, 0xFFFFAF7B, null, new[] { SongTagToken.FourLanes }),
            ("Velvet Static", "Chrome Lullaby", 312, true, 0xFF134E5E, 0xFF71B280, GameCharacter.MonoPro, new SongTagToken[0]),
            ("Paper Comets", "Midnight Telemetry", 236, false, 0xFF1F1C2C, 0xFF928DAB, null, new[] { SongTagToken.SidewinderCamera }),
            ("Hollow Arcade", "Glass Orbit", 187, true, 0xFFC31432, 0xFF240B36, GameCharacter.EraserElite, new SongTagToken[0]),
            ("Neon Harbor", "Overpass Rain", 275, false, 0xFF232526, 0xFF4CA1AF, null, new SongTagToken[0]),
        };
        var seed = 0;
        foreach (var (artist, title, seconds, flac, a, b, character, tags) in tracks)
        {
            var stem = Path.Combine(music, $"{artist} - {title}");
            var audio = stem + (flac ? ".flac" : ".mp3");
            if (flac)
                Flac(audio, seconds);
            else
                Mp3(audio, seconds);
            Cover(stem + ".png", a, b, ++seed);
            var entry = new PlaylistEntry { FilePath = audio, ArtistName = artist, SongTitle = title, CoverPath = stem + ".png", Character = character };
            foreach (var tag in tags)
                entry.Tags.Add(new PlaylistTag { Token = tag });
            night.Entries.Add(entry);
        }
        night.Save();

        foreach (var (name, count, id) in new[] { ("Puzzle Grind", 12, 2), ("Mono Warmup", 5, 3) })
        {
            var list = new Playlist { Id = new Guid($"00000000-0000-0000-0000-{id:D12}"), Name = name };
            for (var i = 0; i < count; i++)
                list.Entries.Add(new PlaylistEntry { FilePath = Path.Combine(music, $"{name} {i}.mp3"), ArtistName = "Various", SongTitle = $"{name} {i + 1}" });
            list.Save();
        }
    }

    // One MPEG-1 Layer III frame (320 kbps, 44.1 kHz, joint stereo) carrying a Xing header: TagLib takes the duration
    // and the bitrate from it, so a kilobyte reads as a whole song.
    static void Mp3(string path, int seconds)
    {
        var frame = new byte[1044];
        frame[0] = 0xFF;
        frame[1] = 0xFB;
        frame[2] = 0xE0;
        frame[3] = 0x40;
        var frames = (int)(seconds * 44100L / 1152);
        var xing = 36;
        "Xing"u8.CopyTo(frame.AsSpan(xing));
        WriteBig(frame, xing + 4, 3); // frames + bytes present
        WriteBig(frame, xing + 8, frames);
        WriteBig(frame, xing + 12, seconds * 40000);
        File.WriteAllBytes(path, frame);
    }

    // "fLaC" and a lone STREAMINFO block: 44.1 kHz, stereo, 16 bit, and the total sample count the duration comes from.
    static void Flac(string path, int seconds)
    {
        var bytes = new byte[4 + 4 + 34];
        "fLaC"u8.CopyTo(bytes);
        bytes[4] = 0x80; // last metadata block, STREAMINFO
        bytes[7] = 34;
        var info = bytes.AsSpan(8);
        info[0] = 0x10; // min/max block size 4096
        info[2] = 0x10;
        long samples = seconds * 44100L;
        // sample rate (20 bits) | channels - 1 (3 bits) | bits per sample - 1 (5 bits) | total samples (36 bits)
        ulong packed = (44100UL << 44) | (1UL << 41) | (15UL << 36) | (ulong)samples;
        for (var i = 0; i < 8; i++)
            info[10 + i] = (byte)(packed >> (56 - 8 * i));
        // TagLib reports no duration for a stream with no audio after the metadata.
        File.WriteAllBytes(path, bytes.Concat(new byte[1024]).ToArray());
    }

    static void WriteBig(byte[] buffer, int offset, int value)
    {
        buffer[offset] = (byte)(value >> 24);
        buffer[offset + 1] = (byte)(value >> 16);
        buffer[offset + 2] = (byte)(value >> 8);
        buffer[offset + 3] = (byte)value;
    }

    static void SeedPalettes(string path)
    {
        static string C(uint argb) =>
            $"{{ \"A\": 255, \"R\": {(argb >> 16) & 0xFF}, \"G\": {(argb >> 8) & 0xFF}, \"B\": {argb & 0xFF} }}";
        (string name, uint purple, uint blue, uint green, uint yellow, uint red)[] palettes =
        {
            ("Classic", 0x800080, 0x0000FF, 0x00FF00, 0xFFFF00, 0xFF0000),
            ("Cyberpunk", 0xB517FF, 0x00E5FF, 0x39FF88, 0xFFE600, 0xFF2A6D),
            ("Pastel Night", 0xC3A6FF, 0x8EC5FF, 0xA8E6CF, 0xFFE3A3, 0xFF9AA2),
            ("Sunset Strip", 0x6A0572, 0x3D5AFE, 0x00BFA5, 0xFFB300, 0xFF3D00),
            ("Mono Contrast", 0x7E57C2, 0x42A5F5, 0x66BB6A, 0xFFEE58, 0xEF5350),
        };
        var entries = palettes.Select(p =>
            $"{{ \"Id\": \"{Guid.NewGuid()}\", \"Name\": \"{p.name}\", \"Purple\": {C(p.purple)}, \"Blue\": {C(p.blue)}, "
            + $"\"Green\": {C(p.green)}, \"Yellow\": {C(p.yellow)}, \"Red\": {C(p.red)} }}");
        File.WriteAllText(path, "{ \"ColorPalettes\": [ " + string.Join(", ", entries) + " ] }");
    }

    // --- per shot ---------------------------------------------------------------------------------------------

    // State that something in the background may have moved since the build: the bridge that never started
    // flips Command and Control back to "not connected", and Settings' theme switch has to match the theme shot.
    static void Restage(Shell shell, ThemeVariant theme)
    {
        Set(shell.Tweaker, "isAudiosurfConnected", true, "IsAudiosurfConnected");
        Set(shell.Settings, "isDarkTheme", theme == ThemeVariant.Dark, "IsDarkTheme");
        Set(shell.Settings, "isOverlaySyncEnabled", true, "PluginStatus");
    }

    static void Arrange(Window window, string tab, Shell shell)
    {
        switch (tab)
        {
            case "skin-changer":
            {
                // The selected skin as the last whole row, with whole rows above it.
                var list = window.GetVisualDescendants().OfType<ListBox>().First(l => ReferenceEquals(l.ItemsSource, shell.Skins.Skins));
                var index = shell.Skins.Skins.IndexOf(shell.Skins.SelectedItem);
                var top = list.ContainerFromIndex(Math.Max(0, index - 4));
                var scroller = list.GetVisualDescendants().OfType<ScrollViewer>().First();
                if (top != null)
                    scroller.Offset = new Vector(0, top.Bounds.Top);
                break;
            }
            case "settings":
            {
                // Down to the plugin card - the top of the tab is paths and switches.
                var scroller = window.GetVisualDescendants().OfType<ScrollViewer>().First(s => s.FindAncestorOfType<TweakerUI.Views.SettingView>() != null);
                var card = window.GetVisualDescendants().OfType<TextBlock>().First(t => t.Text == "In-game plugin");
                var y = card.TranslatePoint(new Point(0, 0), (Visual)scroller.Content!)!.Value.Y;
                scroller.Offset = new Vector(0, Math.Max(0, y - 24));
                break;
            }
        }
    }

    // Paths the stand made up stay out of the pictures: the temp folder becomes a projects folder under a neutral
    // user, and so does the account name in any other path (the editors the machine really has).
    static void Scrub(Window window, string root)
    {
        var profile = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        foreach (var text in window.GetVisualDescendants().OfType<TextBlock>())
        {
            var value = text.Text;
            if (string.IsNullOrEmpty(value))
                continue;
            var scrubbed = value.Replace(Path.Combine(root, "Projects"), @"C:\Users\you\Documents\Audiosurf Scripts", StringComparison.OrdinalIgnoreCase)
                .Replace(Path.Combine(root, "Audiosurf"), ShownGame, StringComparison.OrdinalIgnoreCase)
                .Replace(profile, @"C:\Users\you", StringComparison.OrdinalIgnoreCase);
            if (scrubbed != value)
                text.Text = scrubbed;
        }
    }

    static void Cover(string path, uint from, uint to, int seed)
    {
        const int size = 256;
        using var surface = SKSurface.Create(new SKImageInfo(size, size));
        var canvas = surface.Canvas;
        using (var shader = SKShader.CreateLinearGradient(new SKPoint(0, 0), new SKPoint(size, size), new[] { new SKColor(from), new SKColor(to) }, SKShaderTileMode.Clamp))
        using (var paint = new SKPaint { Shader = shader })
            canvas.DrawRect(0, 0, size, size, paint);
        var random = new Random(seed);
        using (var paint = new SKPaint { IsAntialias = true, Style = SKPaintStyle.Stroke, StrokeWidth = 3, Color = new SKColor(255, 255, 255, 70) })
        {
            for (var i = 0; i < 6; i++)
                canvas.DrawCircle(random.Next(size), random.Next(size), 30 + random.Next(120), paint);
        }
        using var image = surface.Snapshot();
        using var data = image.Encode(SKEncodedImageFormat.Png, 90);
        File.WriteAllBytes(path, data.ToArray());
    }

    // --- helpers ---------------------------------------------------------------------------------------------

    static void SetSetting(string name, object value) =>
        Settings.GetProperty(name, BindingFlags.Static | BindingFlags.NonPublic)!.SetValue(null, value);

    // Writes an [ObservableProperty]'s backing field and raises its change - without the setter's side effects
    // (a tweak's setter sends a command to the game, a path's setter rewrites the settings file).
    static void Set(object vm, string field, object value, string property)
    {
        vm.GetType().GetField(field, BindingFlags.Instance | BindingFlags.NonPublic)!.SetValue(vm, value);
        typeof(CommunityToolkit.Mvvm.ComponentModel.ObservableObject)
            .GetMethod("OnPropertyChanged", BindingFlags.Instance | BindingFlags.NonPublic, new[] { typeof(PropertyChangedEventArgs) })!
            .Invoke(vm, new object[] { new PropertyChangedEventArgs(property) });
    }

    static void DropTransitions(Visual root)
    {
        foreach (var v in new[] { root }.Concat(root.GetVisualDescendants()))
            if (v is Animatable a)
                a.Transitions = null;
    }

    static void Settle()
    {
        for (var i = 0; i < 5; i++)
            Dispatcher.UIThread.RunJobs();
    }

    static void Pump(int ms)
    {
        var clock = Stopwatch.StartNew();
        Pump(() => clock.ElapsedMilliseconds >= ms, ms + 1000);
    }

    static void Pump(Func<bool> done, int timeoutMs)
    {
        var clock = Stopwatch.StartNew();
        while (!done() && clock.ElapsedMilliseconds < timeoutMs)
        {
            Dispatcher.UIThread.RunJobs();
            System.Threading.Thread.Sleep(10);
        }
        Settle();
    }

    static void Await(Task task)
    {
        Pump(() => task.IsCompleted, 60000);
        if (task.IsFaulted)
            throw task.Exception!;
    }

    static void CopyTree(string from, string to)
    {
        foreach (var file in Directory.EnumerateFiles(from, "*", SearchOption.AllDirectories))
        {
            var target = Path.Combine(to, Path.GetRelativePath(from, file));
            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            File.Copy(file, target, true);
        }
    }

    static void TryDelete(string dir)
    {
        try
        {
            if (Directory.Exists(dir))
                Directory.Delete(dir, true);
        }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
    }
}
