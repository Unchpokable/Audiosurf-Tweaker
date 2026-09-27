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
        // Why the exe's own folder and why resolved this way - see AppPaths.
        internal static readonly string AppDirectory = AppPaths.AppDirectory;
        internal static readonly string SkinsRootPath = AppPaths.InAppDirectory("Skins");

        public SkinChangerViewModel()
        {
            ShouldInstallSkyspheres = true;
            ShouldInstallTileset = true;
            ShouldInstallRings = true;
            ShouldInstallParticles = true;
            ShouldInstallHits = true;
            ReloadButtonUnlocked = true;

            Skins = new ObservableCollection<SkinCard>();
            // Single hook for the overlay's SKIN_LIST sync, rather than patching every Skins.Add/Remove
            // call site across this file - catches Add/Remove/Reset alike.
            Skins.CollectionChanged += (_, _) => PushSkinListToOverlay();

            if (EnvironmentChecker.CheckEnvironment(SettingsProvider.GameTexturesPath, out FolderHashInfo state))
                CurrentInstalledSkinRaw = state.StateName;

            OverlayHelper.OverlayReady += (_, _) => PushOverlaySnapshot();
            OverlayHelper.SkinRequested += OnOverlaySkinRequested;

            _ = LoadSkinsAsync();
        }

        private void PushOverlaySnapshot()
        {
            PushSkinListToOverlay();
            OverlayHelper.PushCurrentSkin(CurrentInstalledSkinRaw);
        }

        private void PushSkinListToOverlay() => OverlayHelper.PushSkinList(Skins.Select(c => c.Name));

        // Called by SkinCard once a rename is on disk. Skins.CollectionChanged doesn't see a card renamed
        // in place, and the installed-skin state is keyed by name - renaming the current skin used to
        // leave the tab (and the overlay) showing the old name, or "Unsaved" after the next restart's
        // lookup. Names are unique across cards (SkinCard.ApplyRename refuses a taken one), which is
        // what makes matching by name sound here.
        internal void OnSkinRenamed(string oldName, string newName)
        {
            PushSkinListToOverlay();

            var texturesPath = SettingsProvider.GameTexturesPath;
            if (!string.IsNullOrEmpty(texturesPath) && Directory.Exists(texturesPath))
            {
                try
                {
                    FolderHashInfo.TryRenameState(texturesPath, oldName, newName);
                }
                catch (Exception ex)
                {
                    Logger.Log("SkinChanger.Rename", $"Failed to carry the rename '{oldName}' -> '{newName}' over to the folder state in '{texturesPath}': {ex}");
                }
            }

            if (CurrentInstalledSkinRaw == oldName)
                CurrentInstalledSkinRaw = newName;
        }

        // Reverse-sync (Docs/Internal/overlay-protocol.md, NOTIFY_SKIN): the user pressed "Apply" on
        // a skin directly in the in-game overlay's Skins tab. Mirrors InstallSelectedInternal(clear:
        // false) below - the desktop "Install" button's own defaults - just resolving the target by
        // name instead of SelectedItem, since the overlay's own selection is local to the plugin.
        // An unrecognized name (stale SKIN_LIST snapshot, race with a rename/removal) is silently
        // ignored - the plugin's own pending request will simply time out and show a notefeed toast.
        private void OnOverlaySkinRequested(object sender, string name)
        {
            var card = Skins.FirstOrDefault(c => c.Name == name);
            if (card == null || !File.Exists(card.PathToOrigin))
                return;

            InstallSkin(card.PathToOrigin, SettingsProvider.GameTexturesPath);
        }

        public ObservableCollection<SkinCard> Skins { get; }

        public ObservableCollection<InteractableScreenshot> SelectedItemScreenshots => _selectedScreenshots;

        // By default only the selected skin's screenshots are in memory - re-selecting a skin reads them
        // from its archive again (the screenshots only, SkinPackager.ReadPreviews: a skin's textures, 4k
        // skyspheres included, stay packed until it's installed). Fast Previews is the opt-in opposite -
        // every skin's screenshots pre-decoded, see SetFastPreview. These skeleton placeholder counts match
        // the mockup's own hint-placeholder-count values and just give the reload something to look at
        // instead of a blank list while it's in flight.
        public IReadOnlyList<int> ScreenshotPlaceholders { get; } = Enumerable.Range(0, 4).ToList();
        public IReadOnlyList<int> SkinPlaceholders { get; } = Enumerable.Range(0, 5).ToList();

        private readonly ObservableCollection<InteractableScreenshot> _selectedScreenshots = new();

        // Whether _selectedScreenshots were read for the selection alone (and are disposed when it moves
        // on) or are the selected card's own Fast Previews set (which the card keeps).
        private bool _selectionOwnsScreenshots;

        [ObservableProperty]
        private SkinCard selectedItem;

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

        partial void OnCurrentInstalledSkinRawChanged(string value)
        {
            OnPropertyChanged(nameof(CurrentInstalledSkin));
            OverlayHelper.PushCurrentSkin(value);
        }

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
                    ApplicationNotificationManager.Manager.ShowError("Installation error", $"Given path does not exist: {pathToOrigin}\nThe skin file may have been moved or deleted. Reload the skins list and try again");
                    return;
                }

                using var status = StatusService.Manager.Begin(StatusToken.DiskProcess, "Skin Changer", $"Installing {Path.GetFileName(pathToOrigin)}...");

                if (target.Equals(SettingsProvider.GameTexturesPath, StringComparison.InvariantCultureIgnoreCase))
                {
                    if (SettingsProvider.SafeInstall && !EnvironmentChecker.CheckEnvironment(target, out FolderHashInfo _))
                    {
                        ApplicationNotificationManager.Manager.ShowWarning("Skin installation restriction", "Current texture set is unsaved. Skin installation prohibited");
                        return;
                    }

                    if (SettingsProvider.ControlSystemActive && !EnvironmentChecker.CheckEnvironment(target, out FolderHashInfo _))
                    {
                        if (!await ApplicationNotificationManager.Manager.AskForAction("Danger action warning",
                                "Your current texture set is unsaved. Installing a skin will overwrite unsaved changes and you will lose them. Do you want to continue?"))
                        {
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
            ReleaseCardScreenshots(new[] { target });

            if (!await ApplicationNotificationManager.Manager.AskForAction("Remove Skin", "Do you want to remove file too?"))
                return;

            using (StatusService.Manager.Begin(StatusToken.DiskProcess, "Skin Changer", $"Removing {target.Name}..."))
                await Task.Run(() => File.Delete(target.PathToOrigin));
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
            // The manifest is all that's read: it tells a skin from any other file and gives the card its
            // name. Decoding the whole skin here (as this used to) cost up to ~200 MB of pixels for nothing.
            var name = await Task.Run(() => SkinPackager.ReadName(path));
            if (name == null)
            {
                ApplicationNotificationManager.Manager.ShowError("Error", "Given file is not an Audiosurf Tweaker skin");
                return;
            }

            var newPath = Path.Combine(SkinsRootPath, Path.GetFileName(path));
            if (File.Exists(newPath))
            {
                if (!await ApplicationNotificationManager.Manager.AskForAction("Skin Exists", "Given skin already in skins list. Do you want to overwrite it?"))
                    return;

                File.Delete(newPath);
                var existing = Skins.FirstOrDefault(card => card.Name == name);
                if (existing != null)
                {
                    Skins.Remove(existing);
                    ReleaseCardScreenshots(new[] { existing });
                }
            }

            await Task.Run(() => File.Move(path, newPath));

            var card = new SkinCard(name, newPath, this);
            Skins.Add(card);
            SelectedItem = card;
            FillFastPreviewsIfEnabled();

            ApplicationNotificationManager.Manager.ShowInformation("", "Operation completed!");
        }

        private const string ExportedSkinBaseName = "New exported skin";

        [RelayCommand]
        private async Task ImportFromGame()
        {
            // SkinPackager.CreateSkinFromFolder calls Directory.GetFiles on this path unguarded - an
            // unset/missing game textures folder threw straight out of the background Task and crashed
            // the app instead of showing a normal error.
            var texturesPath = SettingsProvider.GameTexturesPath;
            if (string.IsNullOrEmpty(texturesPath) || !Directory.Exists(texturesPath))
            {
                ApplicationNotificationManager.Manager.ShowError("Import error", "Game textures folder does not exist. Check that 'Path to game textures' setting is valid, then try again");
                return;
            }

            // A fixed name used to overwrite the previous export (and drop its card) if it hadn't been
            // renamed yet.
            var name = ExportedSkinBaseName;
            for (var i = 2; File.Exists(ExportedSkinFile(name)) || Skins.Any(c => c.Name == name); i++)
                name = $"{ExportedSkinBaseName} ({i})";
            var file = ExportedSkinFile(name);

            // The card only appears once its file is fully on disk: it enters rename mode right away, and
            // a rename started while the archive was still being written ran against a missing/partial file.
            SkinCard card = null;
            var stateSaved = false;
            using (StatusService.Manager.Begin(StatusToken.DiskProcess, "Skin Changer", "Exporting current textures..."))
            {
                (card, stateSaved) = await Task.Run(() => ExportTextures(texturesPath, name, file));
            }

            if (card == null)
            {
                ApplicationNotificationManager.Manager.ShowError("Error", "Something went wrong when packing your current texture set. Check application settings and game textures folder");
                return;
            }

            // The exported textures ARE the installed set now - without this the tab kept saying "Unsaved"
            // and the next install warned about losing textures that had just been saved.
            if (stateSaved)
                CurrentInstalledSkinRaw = name;

            Skins.Add(card);
            SelectedItem = card;
            FillFastPreviewsIfEnabled();
            card.EnableRenameCommand.Execute(null);
        }

        private static string ExportedSkinFile(string name) => Path.Combine(SkinsRootPath, name + EnvironmentalVeriables.ActualSkinExtention);

        private (SkinCard card, bool stateSaved) ExportTextures(string texturesPath, string name, string file)
        {
            using var skin = SkinPackager.CreateSkinFromFolder(texturesPath);
            if (skin == null)
                return (null, false);

            skin.Name = name;
            // Hashed right after reading, so the snapshot describes exactly what went into the skin.
            var state = FolderHashInfo.Create(texturesPath, name);

            if (!SkinPackager.CompileToFile(skin, file))
                return (null, false);

            var stateSaved = false;
            try
            {
                state.Save(texturesPath);
                stateSaved = true;
            }
            catch (Exception ex)
            {
                Logger.Log("SkinChanger.Export", $"Exported '{file}' but could not save the folder state to '{texturesPath}': {ex}");
            }

            return (new SkinCard(name, file, this), stateSaved);
        }

        [RelayCommand]
        private Task ReloadSkins() => LoadSkinsAsync();

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
                ApplicationNotificationManager.Manager.ShowWarning("Skin file not found", "The skin file is no longer where it was loaded from. Skins will be reloaded");
                _ = LoadSkinsAsync();
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

                // Dispose frees the decoded textures' native pixels right here - no collection needed (a
                // forced GC.Collect used to follow, and freed nothing Dispose hadn't).
                string installedSkinName = skin.Name;
                skin.Dispose();

                return installedSkinName;
            });
        }

        // The list is built from each archive's manifest.json alone - tens of milliseconds for a folder of
        // skins. It used to decode every skin in full (1.7 GB of pixels for 18 skins) or, to avoid that, keep
        // load.cache: a 36 MB JSON of base64 screenshots whose parsing alone left 128+ MB of pooled buffers
        // behind for the life of the process (Docs/Internal/avalonia-pitfalls.md, §6), and which every rename,
        // removal, export and import had to decode and re-encode in full.
        // A load started while another is still reading (a reload triggered by a missing file mid-load) makes
        // the earlier one's result stale - without this both cleared the list and then both filled it.
        private int _loadGeneration;

        private async Task LoadSkinsAsync()
        {
            var load = ++_loadGeneration;
            LoadingVisible = true;
            ReloadButtonUnlocked = false;

            // A fill still running belongs to the list being replaced.
            _fastPreviewGeneration++;
            ReleaseCardScreenshots(Skins);
            Skins.Clear();

            using (var status = StatusService.Manager.Begin(StatusToken.DiskProcess, "Skin Changer", "Loading skins..."))
            {
                var cards = await Task.Run(() => ReadSkinCards(status));
                if (load != _loadGeneration)
                    return;

                TotalSkinsToLoad = cards.Count;
                CurrentLoadStep = 0;
                foreach (var card in cards)
                {
                    Skins.Add(card);
                    CurrentLoadStep++;
                }
            }

            LoadingVisible = false;
            ReloadButtonUnlocked = true;
            FillFastPreviewsIfEnabled();
        }

        private List<SkinCard> ReadSkinCards(StatusHandle status)
        {
            var skinsRootExists = Directory.Exists(SkinsRootPath);
            // AppContext.BaseDirectory and Environment.CurrentDirectory are logged purely for
            // comparison, not used for path resolution (see AppPaths) -
            // AppContext.BaseDirectory in particular pointed at a %TEMP%\.net\... self-extraction folder
            // under Deploy.ps1's single-file publish, which is exactly what this line's own output
            // caught the first time this was diagnosed.
            Logger.Log("SkinChanger.LoadSkins",
                $"SkinsRootPath='{SkinsRootPath}' (exists={skinsRootExists}); AppDirectory='{AppDirectory}'; " +
                $"AppContext.BaseDirectory='{AppContext.BaseDirectory}'; Environment.CurrentDirectory='{Environment.CurrentDirectory}'; " +
                $"AdditionalSkinsFolderPath='{SettingsProvider.SkinsFolderPath}' (exists={Directory.Exists(SettingsProvider.SkinsFolderPath)})");

            DeleteLegacyLoadingCache();

            var cards = new List<SkinCard>();
            if (!skinsRootExists)
            {
                Dispatcher.UIThread.Post(() => ApplicationNotificationManager.Manager.ShowError("File system error", "Root skins directory not found. Unable to load"));
                return cards;
            }

            var files = CollectSkinFiles();
            Logger.Log("SkinChanger.LoadSkins", $"Collected {files.Count} skin file(s) from '{SkinsRootPath}' (+ additional folder if configured)");

            void OnConversionStarted(string path) => status.Update(StatusToken.DiskProcess, $"Converting legacy skin {Path.GetFileName(path)}...");
            LegacyConverter.ConversionStarted += OnConversionStarted;
            try
            {
                for (var i = 0; i < files.Count; i++)
                {
                    status.Update($"Loading {Path.GetFileName(files[i])} ({i + 1}/{files.Count})...");
                    var name = SkinPackager.ReadName(files[i]);
                    if (name != null)
                        cards.Add(new SkinCard(name, files[i], this));
                }
            }
            finally
            {
                LegacyConverter.ConversionStarted -= OnConversionStarted;
            }

            return cards.OrderBy(c => c.Name, StringComparer.CurrentCultureIgnoreCase).ToList();
        }

        // Left next to the exe by every version before the cache was dropped; nothing reads it any more.
        private static void DeleteLegacyLoadingCache()
        {
            try
            {
                File.Delete(Path.Combine(AppDirectory, "load.cache"));
            }
            catch (Exception ex)
            {
                Logger.Log("SkinChanger.LoadSkins", $"Could not delete the obsolete load.cache: {ex.Message}");
            }
        }

        private static List<string> CollectSkinFiles()
        {
            var files = Directory.EnumerateFiles(SkinsRootPath).ToList();
            if (Directory.Exists(SettingsProvider.SkinsFolderPath))
                files.AddRange(Directory.EnumerateFiles(SettingsProvider.SkinsFolderPath));
            return files;
        }

        // Bumped by every refresh, so a read that finishes after the selection has moved on (or after a
        // newer read for the same card) knows it lost - comparing paths let two reads for the same card,
        // A -> B -> A, both land in the panel.
        private int _screenshotsRequest;

        private async Task RefreshSelectedScreenshotsAsync(SkinCard item)
        {
            var request = ++_screenshotsRequest;
            ClearSelectedScreenshots();
            if (item == null)
            {
                IsLoadingScreenshots = false;
                return;
            }

            // What the card holds decides, not the current value of the Fast Previews setting: right
            // after a runtime switch the two disagree until SetFastPreview has filled/cleared every card,
            // and asking the setting used to show nothing at all for cards loaded before it was turned on.
            if (item.Screenshots != null)
            {
                IsLoadingScreenshots = false;
                foreach (var s in item.Screenshots)
                    _selectedScreenshots.Add(s);
                return;
            }

            IsLoadingScreenshots = true;
            var path = item.PathToOrigin;
            var screenshots = await Task.Run(() => ReadScreenshots(path));
            if (request != _screenshotsRequest)
            {
                // A newer call owns the panel (and IsLoadingScreenshots) now.
                foreach (var s in screenshots)
                    s.Dispose();
                return;
            }

            foreach (var s in screenshots)
                _selectedScreenshots.Add(s);
            _selectionOwnsScreenshots = true;
            IsLoadingScreenshots = false;
        }

        // Takes the panel's screenshots off screen, then disposes them if they were the selection's own.
        // In that order: disposing a bitmap an Image still draws throws on its next redraw (pitfalls §6).
        private void ClearSelectedScreenshots()
        {
            var owned = _selectionOwnsScreenshots ? _selectedScreenshots.ToList() : null;
            _selectedScreenshots.Clear();
            _selectionOwnsScreenshots = false;
            owned?.ForEach(s => s.Dispose());
        }

        // Drops the Fast Previews screenshots of the given cards and disposes them. The panel may be showing
        // one of these very sets (Fast Previews shares the card's own) - it's cleared first, same reason as
        // above; nulling Screenshots releases the card's hover tooltip (Cover) the same way.
        private void ReleaseCardScreenshots(IEnumerable<SkinCard> cards)
        {
            var dropped = new List<InteractableScreenshot>();
            foreach (var card in cards)
            {
                if (card.Screenshots == null)
                    continue;
                dropped.AddRange(card.Screenshots);
                card.Screenshots = null;
            }

            if (dropped.Count == 0)
                return;

            if (!_selectionOwnsScreenshots && _selectedScreenshots.Any(dropped.Contains))
                _selectedScreenshots.Clear();
            foreach (var s in dropped)
                s.Dispose();
        }

        // Fast Previews is a pre-cache, not a lazy one: with it on, every screenshot of every skin is in
        // memory from the moment the list is loaded, so selecting a card and the hover preview never
        // touch the disk - instant response, paid for in memory (~1.5 MB per screenshot). Filled in the
        // background from the archives (screenshots only) after the list loads, and switched at runtime
        // from Settings (via MainWindowViewModel): on fills every card that doesn't have them yet, off
        // drops and disposes them all. The generation counter lets a later switch or reload abandon a
        // fill still in flight.
        private int _fastPreviewGeneration;

        internal void SetFastPreview(bool enabled)
        {
            var generation = ++_fastPreviewGeneration;
            if (enabled)
            {
                _ = FillFastPreviewsAsync(generation);
                return;
            }

            ReleaseCardScreenshots(Skins);

            // The selected card may have been showing its dropped set - reload it the normal way.
            _ = RefreshSelectedScreenshotsAsync(SelectedItem);
        }

        // For a card added to a list that is already filled (import, export). Joins the running generation
        // rather than starting a new one, so a fill still in progress isn't abandoned for it; whichever
        // fill reaches a card first gives it its screenshots.
        private void FillFastPreviewsIfEnabled()
        {
            if (SettingsProvider.UseFastPreview)
                _ = FillFastPreviewsAsync(_fastPreviewGeneration);
        }

        private async Task FillFastPreviewsAsync(int generation)
        {
            var cards = Skins.Where(c => c.Screenshots == null).ToList();
            if (cards.Count == 0)
                return;

            using var status = StatusService.Manager.Begin(StatusToken.DiskProcess, "Skin Changer", "Preparing fast previews...");
            for (var i = 0; i < cards.Count; i++)
            {
                if (generation != _fastPreviewGeneration)
                    return;

                var card = cards[i];
                var path = card.PathToOrigin;
                status.Update($"Preparing fast previews ({i + 1}/{cards.Count})...");

                var screenshots = await Task.Run(() => ReadScreenshots(path));

                // Switched off or reloaded meanwhile, the card got its screenshots from another fill, or it
                // was removed from the list.
                if (generation != _fastPreviewGeneration || card.Screenshots != null || !Skins.Contains(card))
                {
                    foreach (var s in screenshots)
                        s.Dispose();
                    if (generation != _fastPreviewGeneration)
                        return;
                    continue;
                }

                card.Screenshots = new ObservableCollection<InteractableScreenshot>(screenshots);
            }
        }

        private static List<InteractableScreenshot> ReadScreenshots(string pathToSkin) =>
            SkinPackager.ReadPreviews(pathToSkin, bitmap => new InteractableScreenshot(bitmap.ToScreenshot()));
    }
}
