using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using AudiosurfInterface;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SkiaSharp;
using TweakerCore;
using TweakerCore.Engine;
using TweakerCore.FolderChecker;
using TweakerUI.Core;
using TweakerUI.Core.Utils;
using TweakerUI.Models;
using TweakerUI.Services;

namespace TweakerUI.ViewModels
{
    // Ported from SkinChangerRestyle.MVVM.ViewModel.SkinChangerViewModel. Biggest structural change:
    // the old code marshalled background-thread work back to the UI thread via a manual
    // BindingOperations.EnableCollectionSynchronization + MainWindow.WindowDispatcher.Invoke pair;
    // here every mutation of a bound property/collection goes through Dispatcher.UIThread.InvokeAsync
    // instead (architecture rule 3), which also means the singleton-via-static-Instance pattern isn't
    // needed anymore - MainWindowViewModel just owns one instance directly, same as the other modules.
    // "Edit on Disk" mode is dropped entirely - not part of this revival (see roadmap Phase 4.3 notes).
    public partial class SkinChangerViewModel : ViewModelBase
    {
        // Path.GetDirectoryName(Environment.ProcessPath), NOT AppContext.BaseDirectory and NOT a bare
        // relative "Skins" literal - both of the latter broke under Deploy.ps1's self-contained
        // single-file publish, confirmed by adding the logging below and reading it back:
        //   SkinsRootPath='C:\Users\...\AppData\Local\Temp\.net\TweakerUI\<hash>\Skins' (exists=False)
        // A relative "Skins" path resolves against the process's current working directory, not the
        // exe's own folder - those only coincide by accident (e.g. double-clicking from the same
        // folder), which is what made the app's behavior depend on launch method/CWD instead of where
        // the exe actually lives. Switching to AppContext.BaseDirectory looked like the fix (it's the
        // commonly-cited single-file-safe alternative to Assembly.Location) but is ALSO wrong here:
        // Deploy.ps1 publishes with IncludeNativeLibrariesForSelfExtract/IncludeAllContentForSelfExtract
        // (needed so SkiaSharp/HarfBuzzSharp's native assets can run at all from inside a single-file
        // bundle), and once content self-extraction is enabled, AppContext.BaseDirectory reports the
        // %TEMP%\.net\TweakerUI\<hash>\ extraction folder, not the folder containing TweakerUI.exe.
        // Environment.ProcessPath is unaffected by self-extraction - it's the actual apphost .exe's own
        // path in every publish mode - confirmed by ConfigurationManager.ExePath (already on
        // Environment.ProcessPath) correctly landing TweakerUI.exe.config next to the real exe under
        // the exact same single-file bundle.
        internal static readonly string AppDirectory = Path.GetDirectoryName(Environment.ProcessPath);
        internal static readonly string SkinsRootPath = Path.Combine(AppDirectory, "Skins");

        public SkinChangerViewModel()
        {
            ShouldInstallSkyspheres = true;
            ShouldInstallTileset = true;
            ShouldInstallRings = true;
            ShouldInstallParticles = true;
            ShouldInstallHits = true;
            ReloadButtonUnlocked = true;

            Skins = new ObservableCollection<SkinCard>();

            if (EnvironmentChecker.CheckEnvironment(SettingsProvider.GameTexturesPath, out FolderHashInfo state))
                CurrentInstalledSkinRaw = state.StateName;

            _ = LoadSkinsAsync(rebuildCache: false);
        }

        public ObservableCollection<SkinCard> Skins { get; }

        public ObservableCollection<InteractableScreenshot> SelectedItemScreenshots => _selectedScreenshots;

        // No caching (deliberately - the app already sits around 200MB resident, holding decoded
        // screenshot bitmaps for every skin on top of that isn't worth it), so re-selecting a skin
        // always re-decodes. These skeleton placeholder counts match the mockup's own
        // hint-placeholder-count values and just give the reload something to look at instead of a
        // blank list while it's in flight.
        public IReadOnlyList<int> ScreenshotPlaceholders { get; } = Enumerable.Range(0, 4).ToList();
        public IReadOnlyList<int> SkinPlaceholders { get; } = Enumerable.Range(0, 5).ToList();

        private readonly ObservableCollection<InteractableScreenshot> _selectedScreenshots = new();

        [ObservableProperty]
        private SkinCard selectedItem;

        [ObservableProperty]
        private string changerStatus;

        [ObservableProperty]
        private bool loadingVisible;

        [ObservableProperty]
        private bool isLoadingScreenshots;

        [ObservableProperty]
        private int totalSkinsToLoad;

        [ObservableProperty]
        private int currentLoadStep;

        [ObservableProperty]
        private bool reloadButtonUnlocked;

        [ObservableProperty]
        private bool shouldInstallSkyspheres;

        [ObservableProperty]
        private bool shouldInstallTileset;

        [ObservableProperty]
        private bool shouldInstallParticles;

        [ObservableProperty]
        private bool shouldInstallRings;

        [ObservableProperty]
        private bool shouldInstallHits;

        [ObservableProperty]
        private string currentInstalledSkinRaw;

        public string CurrentInstalledSkin => CurrentInstalledSkinRaw ?? "Unsaved";

        public bool HasSelection => SelectedItem != null;

        partial void OnCurrentInstalledSkinRawChanged(string value) => OnPropertyChanged(nameof(CurrentInstalledSkin));

        partial void OnSelectedItemChanged(SkinCard value)
        {
            OnPropertyChanged(nameof(HasSelection));
            _ = RefreshSelectedScreenshotsAsync(value);
        }

        public async void InstallSkin(string pathToOrigin, string target, bool forced = false, bool unpackScreenshots = false, bool clearInstall = false, bool saveState = true)
        {
            // Suppressed for the whole call, not just around the actual write, matching the donor's
            // placement - without it, the watcher (when enabled) sees the Tweaker's own texture writes
            // as an external change and redundantly reloads/re-snapshots on top of what this method is
            // already doing. try/finally (the donor used neither) guarantees re-enabling on every early
            // return below - without it, a declined "unsaved changes" prompt left the watcher suppressed
            // forever, silently dead until the next full install.
            TexturesWatcher.IfActive?.DisableRaisingEvents();
            try
            {
                if (!Directory.Exists(target))
                {
                    ApplicationNotificationManager.Manager.ShowError("Installation Error", $"Given directory does not exist: {target}\nCheck that 'Path to game textures' setting is valid");
                    return;
                }
                if (!File.Exists(pathToOrigin))
                {
                    ApplicationNotificationManager.Manager.ShowError("Installation error", $"Given path does not exist: {pathToOrigin}\nIt may be caused by corrupted skins cache. Please rebuild skins cache and try again");
                    return;
                }

                ChangerStatus = "Working...";

                if (target.Equals(SettingsProvider.GameTexturesPath, StringComparison.InvariantCultureIgnoreCase))
                {
                    if (SettingsProvider.SafeInstall && !EnvironmentChecker.CheckEnvironment(target, out FolderHashInfo _))
                    {
                        ApplicationNotificationManager.Manager.ShowWarning("Skin installation restriction", "Current texture set is unsaved. Skin installation prohibited");
                        ChangerStatus = "Ready";
                        return;
                    }

                    if (SettingsProvider.ControlSystemActive && !EnvironmentChecker.CheckEnvironment(target, out FolderHashInfo _))
                    {
                        if (!await ApplicationNotificationManager.Manager.AskForAction("Danger action warning",
                                "Your current texture set is unsaved. Installing a skin will overwrite unsaved changes and you will lose them. Do you want to continue?"))
                        {
                            ChangerStatus = "Ready";
                            return;
                        }
                    }
                }

                if (clearInstall)
                {
                    Utils.HardClear(target);
                    AudiosurfHandle.Instance.Command(GameProtocol.Command(GameProtocol.ReloadTextures));
                }

                var skinName = await InstallSkinInternal(pathToOrigin, target, forced, unpackScreenshots, saveState);

                if (SettingsProvider.HotReload)
                    AudiosurfHandle.Instance.Command(GameProtocol.Command(GameProtocol.ReloadTextures));

                ApplicationNotificationManager.Manager.ShowSuccess("Done!", $"Skin \"{skinName}\" successfully installed. Enjoy! ^_^");
                ChangerStatus = "Ready";
            }
            finally
            {
                TexturesWatcher.IfActive?.EnableRaisingEvents();
            }
        }

        public async Task RemoveSkin(SkinCard target)
        {
            if (target == null || !Skins.Contains(target))
                return;

            Skins.Remove(target);

            if (!await ApplicationNotificationManager.Manager.AskForAction("Remove Skin", "Do you want to remove file too?"))
                return;

            ChangerStatus = "Working...";
            await Task.Run(() =>
            {
                File.Delete(target.PathToOrigin);
                var cacheDir = AppDirectory;
                if (LoadingCache.TryFind(cacheDir, out LoadingCache cache))
                {
                    cache.Data.RemoveAll(x => x.Name == target.Name);
                    cache.Serialize(cacheDir);
                    Utils.DisposeAndClear(cache);
                }
            });
            ChangerStatus = "Ready";
            ApplicationNotificationManager.Manager.ShowInformation("", "Operation completed");
        }

        public void HandleFileDrop(IEnumerable<string> filePaths)
        {
            var accepted = filePaths.Where(f => new[] { EnvironmentalVeriables.LegacySkinExtention, EnvironmentalVeriables.ActualSkinExtention }.Any(ext => ext.Equals(Path.GetExtension(f), StringComparison.OrdinalIgnoreCase)));
            foreach (var file in accepted)
                _ = AddNewSkinAsync(file);
        }

        [RelayCommand]
        private async Task AddNewSkin()
        {
            var fileTypes = new[]
            {
                new FilePickerFileType("Tweaker Skin Package")
                {
                    Patterns = new[] { "*" + EnvironmentalVeriables.ActualSkinExtention, "*" + EnvironmentalVeriables.LegacySkinExtention }
                }
            };
            var path = await FileDialogService.OpenFileAsync("Select a skin package", fileTypes);
            if (path == null)
                return;

            await AddNewSkinAsync(path);
        }

        private async Task AddNewSkinAsync(string path)
        {
            var skin = await Task.Run(() => SkinPackager.Decompile(path));
            if (skin == null)
            {
                ApplicationNotificationManager.Manager.ShowError("Error", "Given file is not an Audiosurf Tweaker skin");
                return;
            }

            var newPath = Path.Combine(SkinsRootPath, Path.GetFileName(path));
            if (File.Exists(newPath))
            {
                if (await ApplicationNotificationManager.Manager.AskForAction("Skin Exists", "Given skin already in skins list. Do you want to overwrite it?"))
                {
                    File.Delete(newPath);
                    var existing = Skins.FirstOrDefault(card => card.Name == skin.Name);
                    if (existing != null)
                        Skins.Remove(existing);
                }
                else
                {
                    skin.Dispose();
                    return;
                }
            }

            await Task.Run(() => File.Move(path, newPath));

            var card = new SkinCard(skin, newPath, this);
            Skins.Add(card);
            SelectedItem = card;

            var cacheDir = AppDirectory;
            await Task.Run(() =>
            {
                if (LoadingCache.TryFind(cacheDir, out LoadingCache cache))
                {
                    cache.Data.Add(new LoadedSkinData(skin, newPath));
                    cache.Serialize(cacheDir);
                }
                Utils.DisposeAndClear(cache, skin);
            });

            ApplicationNotificationManager.Manager.ShowInformation("", "Operation completed!");
        }

        [RelayCommand]
        private async Task ImportFromGame()
        {
            // SkinPackager.CreateSkinFromFolder calls Directory.GetFiles on this path unguarded - an
            // unset/missing game textures folder threw straight out of the background Task and crashed
            // the app instead of showing a normal error.
            if (string.IsNullOrEmpty(SettingsProvider.GameTexturesPath) || !Directory.Exists(SettingsProvider.GameTexturesPath))
            {
                ApplicationNotificationManager.Manager.ShowError("Import error", "Game textures folder does not exist. Check that 'Path to game textures' setting is valid, then try again");
                return;
            }

            var skin = await Task.Run(() => SkinPackager.CreateSkinFromFolder(SettingsProvider.GameTexturesPath));
            if (skin == null)
            {
                ApplicationNotificationManager.Manager.ShowError("Error", "Something went wrong when packing your current texture set. Check application settings and game textures folder");
                return;
            }

            skin.Name = "New exported skin";
            var source = Path.Combine(SkinsRootPath, skin.Name + EnvironmentalVeriables.ActualSkinExtention);

            var card = new SkinCard(skin, source, this);
            var existing = Skins.FirstOrDefault(c => c.PathToOrigin.Equals(card.PathToOrigin, StringComparison.OrdinalIgnoreCase));
            if (existing != null)
                Skins.Remove(existing);
            Skins.Add(card);

            card.EnableRenameCommand.Execute(null);

            await Task.Run(() =>
            {
                SkinPackager.CompileToPath(skin, SkinsRootPath);
                Utils.DisposeAndClear(skin);
            });

            SelectedItem = card;
        }

        [RelayCommand]
        private async Task ReloadSkins()
        {
            Skins.Clear();
            await LoadSkinsAsync(rebuildCache: true);
        }

        [RelayCommand]
        private async Task RemoveSelected() => await RemoveSkin(SelectedItem);

        [RelayCommand]
        private void InstallSelected() => InstallSelectedInternal(clear: false);

        [RelayCommand]
        private void ClearInstallSelected() => InstallSelectedInternal(clear: true);

        private void InstallSelectedInternal(bool clear)
        {
            if (SelectedItem == null)
                return;

            if (!File.Exists(SelectedItem.PathToOrigin))
            {
                ApplicationNotificationManager.Manager.ShowWarning("Cache error", "A cache read-write error occurred. Skins will be reloaded");
                _ = LoadSkinsAsync(rebuildCache: true);
                return;
            }

            InstallSkin(SelectedItem.PathToOrigin, SettingsProvider.GameTexturesPath, clearInstall: clear);
        }

        private Task<string> InstallSkinInternal(string pathToOrigin, string target, bool forced, bool unpackScreenshots, bool saveState)
        {
            return Task.Run(() =>
            {
                var skin = SkinPackager.Decompile(pathToOrigin);
                if (skin == null)
                    return null;

                if (ShouldInstallSkyspheres || forced)
                    skin.SkySpheres?.Apply(x => x?.Save(target));

                if (ShouldInstallHits || forced)
                    skin.Hits?.Apply(x => x?.Save(target));

                if (ShouldInstallTileset || forced)
                {
                    skin.Tiles?.Save(target);
                    skin.TilesFlyup?.Save(target);
                }

                if (ShouldInstallParticles || forced)
                    skin.Particles?.Apply(x => x?.Save(target));
                if (ShouldInstallRings || forced)
                    skin.Rings?.Apply(x => x?.Save(target));
                skin.Cliffs?.Apply(x => x?.Save(target));

                if (unpackScreenshots)
                {
                    Directory.CreateDirectory($@"{target}\Screenshots\");
                    skin.Previews?.Apply(x => x?.Save($@"{target}\Screenshots"));
                }

                if (saveState)
                {
                    var state = FolderHashInfo.Create(target, skin.Name);
                    state.Save(target);
                    Dispatcher.UIThread.Post(() => CurrentInstalledSkinRaw = state.StateName);
                }

                string installedSkinName = skin.Name;
                skin.Dispose();
                GC.Collect();

                return installedSkinName;
            });
        }

        private async Task LoadSkinsAsync(bool rebuildCache)
        {
            ChangerStatus = "Loading...";
            LoadingVisible = true;
            ReloadButtonUnlocked = false;

            await Task.Run(() => LoadSkinsCoreAsync(rebuildCache));

            LoadingVisible = false;
            ChangerStatus = "Ready";
            ReloadButtonUnlocked = true;
        }

        private static readonly Logger _logger = new Logger();

        private async Task LoadSkinsCoreAsync(bool rebuildCache)
        {
            var skinsRootExists = Directory.Exists(SkinsRootPath);
            // AppContext.BaseDirectory and Environment.CurrentDirectory are logged purely for
            // comparison, not used for path resolution (see AppDirectory/SkinsRootPath comment above) -
            // AppContext.BaseDirectory in particular pointed at a %TEMP%\.net\... self-extraction folder
            // under Deploy.ps1's single-file publish, which is exactly what this line's own output
            // caught the first time this was diagnosed.
            _logger.Log("SkinChanger.LoadSkins",
                $"SkinsRootPath='{SkinsRootPath}' (exists={skinsRootExists}); AppDirectory='{AppDirectory}'; " +
                $"AppContext.BaseDirectory='{AppContext.BaseDirectory}'; Environment.CurrentDirectory='{Environment.CurrentDirectory}'; " +
                $"AdditionalSkinsFolderPath='{SettingsProvider.SkinsFolderPath}' (exists={Directory.Exists(SettingsProvider.SkinsFolderPath)})");

            if (!skinsRootExists)
            {
                await Dispatcher.UIThread.InvokeAsync(() => ApplicationNotificationManager.Manager.ShowError("File system error", "Root skins directory not found. Unable to load"));
                return;
            }

            var files = CollectSkinFiles();
            var cacheDir = AppDirectory;
            _logger.Log("SkinChanger.LoadSkins", $"Collected {files.Count} skin file(s) from '{SkinsRootPath}' (+ additional folder if configured); cacheDir='{cacheDir}'");

            if (!rebuildCache && LoadingCache.TryFind(cacheDir, out LoadingCache cache) &&
                UnorderedSequenceEquals(files, cache.Data.Select(x => x.PathToOriginFile).ToList()))
            {
                await LoadSkinsFromCacheAsync(cache);
            }
            else
            {
                await LoadSkinsFullAsync(files, cacheDir);
            }
        }

        private async Task LoadSkinsFromCacheAsync(LoadingCache cache)
        {
            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                TotalSkinsToLoad = cache.Data.Count;
                CurrentLoadStep = 0;
            });

            foreach (var cachedSkin in cache.Data.OrderBy(x => x?.Name))
            {
                if (cachedSkin == null)
                {
                    await Dispatcher.UIThread.InvokeAsync(() => ApplicationNotificationManager.Manager.ShowError("", "Cache error occurred. Skins will be loaded from source files"));
                    await LoadSkinsFullAsync(CollectSkinFiles(), AppDirectory);
                    return;
                }

                var card = new SkinCard(cachedSkin, this);
                await Dispatcher.UIThread.InvokeAsync(() =>
                {
                    Skins.Add(card);
                    CurrentLoadStep++;
                });
            }

            Utils.DisposeAndClear(cache);
        }

        private async Task LoadSkinsFullAsync(List<string> files, string cacheDir)
        {
            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                Skins.Clear();
                TotalSkinsToLoad = files.Count;
                CurrentLoadStep = 0;
            });

            var cache = new LoadingCache();
            foreach (var file in files)
            {
                var skin = SkinPackager.Decompile(file);
                if (skin == null)
                    continue;

                cache.Data.Add(new LoadedSkinData(skin, file));
                var card = new SkinCard(skin, file, this);
                skin.Dispose();

                await Dispatcher.UIThread.InvokeAsync(() =>
                {
                    Skins.Add(card);
                    CurrentLoadStep++;
                });
            }

            cache.Serialize(cacheDir);
            Utils.DisposeAndClear(cache);
        }

        private static List<string> CollectSkinFiles()
        {
            var files = Directory.EnumerateFiles(SkinsRootPath).ToList();
            if (Directory.Exists(SettingsProvider.SkinsFolderPath))
                files.AddRange(Directory.EnumerateFiles(SettingsProvider.SkinsFolderPath));
            return files;
        }

        private static bool UnorderedSequenceEquals(List<string> a, List<string> b)
        {
            if (a.Count != b.Count)
                return false;
            return a.OrderBy(x => x).SequenceEqual(b.OrderBy(x => x));
        }

        private async Task RefreshSelectedScreenshotsAsync(SkinCard item)
        {
            _selectedScreenshots.Clear();
            if (item == null)
            {
                IsLoadingScreenshots = false;
                return;
            }

            if (SettingsProvider.UseFastPreview)
            {
                // Already decoded up front when the card was created - nothing to wait on.
                IsLoadingScreenshots = false;
                if (item.Screenshots != null)
                    foreach (var s in item.Screenshots)
                        _selectedScreenshots.Add(s);
                return;
            }

            IsLoadingScreenshots = true;
            var path = item.PathToOrigin;
            var bitmaps = await Task.Run(() => LoadSkinScreenshots(path));
            if (SelectedItem?.PathToOrigin != path)
                return; // selection changed while loading - a newer call owns IsLoadingScreenshots now

            foreach (var bitmap in bitmaps)
            {
                _selectedScreenshots.Add(new InteractableScreenshot(bitmap.ToAvaloniaBitmap()));
                bitmap.Dispose();
            }
            IsLoadingScreenshots = false;
        }

        private static List<SKBitmap> LoadSkinScreenshots(string pathToSkin)
        {
            using var skin = SkinPackager.Decompile(pathToSkin);
            if (skin?.Previews == null)
                return new List<SKBitmap>();

            return skin.Previews.Group.Select(screenshot => ((SKBitmap)screenshot).Rescale(860, 440)).ToList();
        }
    }
}
