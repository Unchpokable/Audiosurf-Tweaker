using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using TweakerUI.Core;
using TweakerUI.Core.Utils;
using TweakerUI.Models;
using TweakerUI.Services;

namespace TweakerUI.ViewModels
{
    public partial class ColorsConfiguratorViewModel : ViewModelBase
    {
        // Next to TweakerUI.exe. Was a bare "palettes", i.e. relative to the working directory - a
        // shortcut with another "Start in" folder made every palette vanish (see AppPaths).
        public static readonly string PaletteContainerFilename = AppPaths.InAppDirectory("palettes");

        private static readonly (ASColors Role, string Name)[] RoleOrder =
        {
            (ASColors.Purple, "Purple"),
            (ASColors.Blue, "Blue"),
            (ASColors.Green, "Green"),
            (ASColors.Yellow, "Yellow"),
            (ASColors.Red, "Red"),
        };

        public ColorsConfiguratorViewModel()
        {
            Swatches = new ObservableCollection<SwatchEditState>(RoleOrder.Select(r => new SwatchEditState(r.Role, r.Name)));

            var existing = PaletteDynamicLoadContainer.Load(PaletteContainerFilename);
            if (existing == null)
            {
                ApplicationNotificationManager.Manager.ShowWarning("Warning",
                    "Palettes storage file was not found or corrupted and has been rewritten by a new empty storage. This is normal on first run.");
                existing = new PaletteDynamicLoadContainer();
            }

            if (existing.ColorPalettes.Count == 0)
                existing.Add(CreateDefaultPalette());

            // Every palette needs its own Id in memory AND on disk - storage finds a palette to replace or
            // remove by Id. Prints older than the Id field (Guid.Empty) get a fresh one in the ColorPalette
            // constructor, and two copies of one imported .palette file shared theirs; both only ever
            // existed in memory, so Remove found nothing on disk (the palette came back on the next start)
            // and Replace appended a duplicate. The container is rebuilt from what was loaded and saved
            // below, which is what writes the settled Ids back.
            var loaded = existing.ColorPalettes.Select(p => new ColorPalette(p)).ToList();
            var seenIds = new HashSet<Guid>();
            foreach (var palette in loaded)
                if (!seenIds.Add(palette.Id))
                    palette.Id = Guid.NewGuid();

            Palettes = new ObservableCollection<ColorPalette>(loaded.OrderBy(p => p.Name));
            var settled = new PaletteDynamicLoadContainer();
            foreach (var palette in Palettes)
                settled.Add(palette);
            PaletteDynamicLoadContainer.Save(settled, PaletteContainerFilename);

            SelectPalette(Palettes.FirstOrDefault());
        }

        private static ColorPalette CreateDefaultPalette() => new ColorPalette
        {
            Name = "New Palette",
            Purple = Colors.Purple,
            Blue = Colors.Blue,
            Green = Colors.Lime,
            Yellow = Colors.Yellow,
            Red = Colors.Red,
        };

        public ObservableCollection<ColorPalette> Palettes { get; }
        public ObservableCollection<SwatchEditState> Swatches { get; }

        [ObservableProperty]
        private ColorPalette editedPalette;

        // Staged, local-until-applied - matches the old WPF app's actual behavior (confirmed by the
        // user after checking; typing here does NOT touch the list row until Apply Changes commits it,
        // and Discard resets it back to EditedPalette.Name).
        [ObservableProperty]
        private string paletteNameInput;

        [ObservableProperty]
        private int editingIndex;

        [ObservableProperty]
        private string colorTab = "HSV";

        public SwatchEditState CurrentSwatch => Swatches.Count > 0 ? Swatches[EditingIndex] : null;

        // Fixed-order convenience accessors for the preview strip (Swatches never gets items added or
        // removed after construction - only its 5 entries' HSV values change).
        public SwatchEditState PurpleSwatch => Swatches[0];
        public SwatchEditState BlueSwatch => Swatches[1];
        public SwatchEditState GreenSwatch => Swatches[2];
        public SwatchEditState YellowSwatch => Swatches[3];
        public SwatchEditState RedSwatch => Swatches[4];

        partial void OnEditingIndexChanged(int value) => OnPropertyChanged(nameof(CurrentSwatch));

        [RelayCommand]
        private void SelectPalette(ColorPalette palette)
        {
            if (palette == null)
                return;

            foreach (var p in Palettes)
                p.IsSelected = ReferenceEquals(p, palette);

            EditedPalette = palette;
            PaletteNameInput = palette.Name;
            Swatches[0].SetFromColor(palette.Purple);
            Swatches[1].SetFromColor(palette.Blue);
            Swatches[2].SetFromColor(palette.Green);
            Swatches[3].SetFromColor(palette.Yellow);
            Swatches[4].SetFromColor(palette.Red);
            EditingIndex = 0;
        }

        [RelayCommand]
        private void SelectSwatch(SwatchEditState swatch)
        {
            var idx = Swatches.IndexOf(swatch);
            if (idx < 0)
                return;

            foreach (var s in Swatches)
                s.IsSelected = false;
            swatch.IsSelected = true;
            EditingIndex = idx;
        }

        [RelayCommand]
        private void SelectTab(string tab) => ColorTab = tab;

        // The mockup's "Инверсия" button rotates hue by 180 degrees (complementary color), not a raw
        // RGB channel invert like the old WPF ToNegative() did - this is a deliberate behavior change
        // from the mockup, kept as designed rather than carried over from the old app.
        [RelayCommand]
        private void MakeNegative()
        {
            foreach (var swatch in Swatches)
                swatch.Hue = (swatch.Hue + 180) % 360;
        }

        [RelayCommand]
        private async Task NewPalette()
        {
            var palette = CreateDefaultPalette();
            Palettes.Add(palette);
            SelectPalette(palette);

            if (!await Task.Run(() => PaletteDynamicLoadContainer.Add(palette, PaletteContainerFilename)))
                await ApplicationNotificationManager.Manager.ShowImportantInfo("Cache error", "Error while writing cached palette storage. Changes will not be saved");
        }

        private ColorPalette BuildWorkingPalette()
        {
            return new ColorPalette
            {
                Id = EditedPalette?.Id ?? Guid.NewGuid(),
                Name = PaletteNameInput,
                Purple = Swatches[0].Color,
                Blue = Swatches[1].Color,
                Green = Swatches[2].Color,
                Yellow = Swatches[3].Color,
                Red = Swatches[4].Color,
            };
        }

        [RelayCommand]
        private async Task ApplyChanges()
        {
            if (EditedPalette == null)
                return;

            var updated = BuildWorkingPalette();
            var ok = await Task.Run(() => PaletteDynamicLoadContainer.Replace(EditedPalette, updated, PaletteContainerFilename));
            if (!ok)
            {
                await ApplicationNotificationManager.Manager.ShowImportantInfo("Cache error", "Error while writing cached palette storage. Changes will not be saved");
                return;
            }

            EditedPalette.Name = updated.Name;
            EditedPalette.Purple = updated.Purple;
            EditedPalette.Blue = updated.Blue;
            EditedPalette.Green = updated.Green;
            EditedPalette.Yellow = updated.Yellow;
            EditedPalette.Red = updated.Red;
        }

        [RelayCommand]
        private async Task SaveAsNew()
        {
            var newPalette = BuildWorkingPalette();
            newPalette.Id = Guid.NewGuid();
            Palettes.Add(newPalette);
            SelectPalette(newPalette);

            var ok = await Task.Run(() => PaletteDynamicLoadContainer.Add(newPalette, PaletteContainerFilename));
            if (!ok)
                await ApplicationNotificationManager.Manager.ShowImportantInfo("Cache error", "Error while writing cached palette storage. Changes will not be saved");
        }

        [RelayCommand]
        private void Discard() => SelectPalette(EditedPalette);

        [RelayCommand]
        private async Task RemoveSelectedPalette()
        {
            if (EditedPalette == null)
                return;

            if (!await ApplicationNotificationManager.Manager.AskForAction("Remove Palette", "This action will delete the selected color palette. Are you sure?"))
                return;

            if (!Palettes.Contains(EditedPalette))
                return;

            var removed = EditedPalette;
            if (!await Task.Run(() => PaletteDynamicLoadContainer.Remove(removed, PaletteContainerFilename)))
                await ApplicationNotificationManager.Manager.ShowImportantInfo("Cache error", "Error while writing cached palette storage. Changes will not be saved");

            var removedIdx = Palettes.IndexOf(removed);
            Palettes.Remove(removed);

            // Never leave the editor without a palette: SelectPalette(null) is a no-op, so removing the
            // last one used to leave EditedPalette pointing at the deleted palette. Same fallback as a
            // first run with empty storage.
            if (Palettes.Count == 0)
            {
                await NewPalette();
                return;
            }

            SelectPalette(Palettes[Math.Min(removedIdx, Palettes.Count - 1)]);
        }

        [RelayCommand]
        private async Task ApplyPaletteToGame()
        {
            // GameTexturesPath can legitimately be null/missing here - that's exactly the state left
            // behind by ConfigurationManager's "can't detect Audiosurf installation" fault path, which
            // points the user at Settings to fix it rather than crashing on the very next click.
            if (string.IsNullOrEmpty(SettingsProvider.GameTexturesPath) || !Directory.Exists(SettingsProvider.GameTexturesPath))
            {
                ApplicationNotificationManager.Manager.ShowError("Error", "Game textures folder is not set or does not exist. Check the Settings tab, then try again");
                return;
            }

            var isGameKilled = false;
            if (GameProcessService.IsRunning)
            {
                if (!await ApplicationNotificationManager.Manager.AskForAction("Game is running",
                        "Audiosurf Tweaker detected a running game instance. Color settings can't be overwritten while the game is running. Shut down the game to rewrite settings? This will start the game back up when the operation completes."))
                {
                    return;
                }

                // Through GameProcessService rather than the "taskkill /f" this used to run: the game writes
                // options.ini on the way out, and this method is about to rewrite that very file - killing it
                // outright meant racing the game's own save of the file being edited (Р-21).
                if (!await GameProcessService.CloseAsync())
                {
                    ApplicationNotificationManager.Manager.ShowError("Error", "Audiosurf could not be closed - the palette was not applied");
                    return;
                }

                isGameKilled = true;
            }

            var pathToIni = Directory.GetParent(SettingsProvider.GameTexturesPath)?.FullName + "\\options.ini";
            var presenter = new AudiosurfConfigurationPresenter();

            try
            {
                presenter.ProcessConfigFile(pathToIni);
                presenter.ApplyPalette(BuildWorkingPalette());
                presenter.SaveChanges();
            }
            catch (Exception e)
            {
                ApplicationNotificationManager.Manager.ShowError("Error", $"Error while writing game configuration file: {e.Message}");
                return;
            }

            if (isGameKilled)
                GameProcessService.Start(Directory.GetParent(SettingsProvider.GameTexturesPath)?.Parent?.FullName);

            ApplicationNotificationManager.Manager.ShowSuccess("Done!", "Operation completed!");
        }

        [RelayCommand]
        private async Task SaveCurrentFromGame()
        {
            if (string.IsNullOrEmpty(SettingsProvider.GameTexturesPath) || !Directory.Exists(SettingsProvider.GameTexturesPath))
            {
                ApplicationNotificationManager.Manager.ShowError("Error", "Game textures folder is not set or does not exist. Check the Settings tab, then try again");
                return;
            }

            var pathToIni = Directory.GetParent(SettingsProvider.GameTexturesPath)?.FullName + "\\options.ini";
            var presenter = new AudiosurfConfigurationPresenter();
            try
            {
                presenter.ProcessConfigFile(pathToIni);
            }
            catch (Exception e)
            {
                ApplicationNotificationManager.Manager.ShowError("Error", $"Error while reading game configuration: {e.Message}");
                return;
            }

            var palette = presenter.ExportPalette();
            palette.Name = "My custom palette";
            Palettes.Add(palette);
            SelectPalette(palette);

            if (!await Task.Run(() => PaletteDynamicLoadContainer.Add(palette, PaletteContainerFilename)))
                await ApplicationNotificationManager.Manager.ShowImportantInfo("Cache error", "Error while writing cached palette storage. Changes will not be saved");
        }

        [RelayCommand]
        private async Task ExportPalette()
        {
            var folder = await FileDialogService.OpenFolderAsync("Select export folder");
            if (folder == null)
                return;

            var palette = BuildWorkingPalette();
            var ok = await Task.Run(() => ColorPalette.Save(palette, folder));
            if (!ok)
                ApplicationNotificationManager.Manager.ShowError("Error", "Error while exporting palette");
            else
                ApplicationNotificationManager.Manager.ShowSuccess("Done!", "Operation completed!");
        }

        [RelayCommand]
        private async Task ImportPalette()
        {
            var fileTypes = new[] { new FilePickerFileType("Color palette") { Patterns = new[] { "*.palette" } } };
            var file = await FileDialogService.OpenFileAsync("Select a palette file", fileTypes);
            if (file == null)
                return;

            var palette = ColorPalette.Load(file);
            if (palette == null)
            {
                ApplicationNotificationManager.Manager.ShowError("Error", "Could not load the selected palette");
                return;
            }

            // A .palette file carries the Id of the palette it was exported from: importing it twice, or
            // back into the app it came from, would give two palettes one Id - and removing either would
            // remove both from storage.
            palette.Id = Guid.NewGuid();
            Palettes.Add(palette);
            SelectPalette(palette);

            if (!await Task.Run(() => PaletteDynamicLoadContainer.Add(palette, PaletteContainerFilename)))
            {
                await ApplicationNotificationManager.Manager.ShowImportantInfo("Cache error", "Error while writing cached palette storage. Changes will not be saved");
                return;
            }
            ApplicationNotificationManager.Manager.ShowInformation("", "Operation completed!");
        }
    }
}
