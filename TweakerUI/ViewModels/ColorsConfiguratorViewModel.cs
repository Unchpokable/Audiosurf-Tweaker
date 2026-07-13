using System;
using System.Collections.ObjectModel;
using System.Diagnostics;
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
        public static readonly string PaletteContainerFilename = "palettes";

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
            {
                existing.Add(new ColorPalette
                {
                    Name = "New Palette",
                    Purple = Colors.Purple,
                    Blue = Colors.Blue,
                    Green = Colors.Lime,
                    Yellow = Colors.Yellow,
                    Red = Colors.Red,
                });
            }

            Palettes = new ObservableCollection<ColorPalette>(existing.ColorPalettes.Select(p => new ColorPalette(p)).OrderBy(p => p.Name));
            PaletteDynamicLoadContainer.Save(existing, PaletteContainerFilename);

            SelectPalette(Palettes.FirstOrDefault());
        }

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
            var palette = new ColorPalette
            {
                Name = "New Palette",
                Purple = Colors.Purple,
                Blue = Colors.Blue,
                Green = Colors.Lime,
                Yellow = Colors.Yellow,
                Red = Colors.Red,
            };
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

            if (!PaletteDynamicLoadContainer.Remove(EditedPalette, PaletteContainerFilename))
                await ApplicationNotificationManager.Manager.ShowImportantInfo("Cache error", "Error while writing cached palette storage. Changes will not be saved");

            var removedIdx = Palettes.IndexOf(EditedPalette);
            Palettes.Remove(EditedPalette);
            SelectPalette(Palettes.Count > 0 ? Palettes[Math.Min(removedIdx, Palettes.Count - 1)] : null);
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
            var gameProcesses = Process.GetProcessesByName("QuestViewer");
            if (gameProcesses.Length > 0)
            {
                if (await ApplicationNotificationManager.Manager.AskForAction("Game is running",
                        "Audiosurf Tweaker detected a running game instance. Color settings can't be overwritten while the game is running. Shut down the game to rewrite settings? This will start the game back up when the operation completes."))
                {
                    Utils.Cmd($"taskkill /f /pid {gameProcesses[0].Id}");
                    isGameKilled = true;
                    await WaitForGameStopWorking("QuestViewer");
                }
                else
                {
                    return;
                }
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
                ApplicationNotificationManager.Manager.ShowErrorWnd("Error", $"Error while writing game configuration file: {e.Message}");
                return;
            }

            if (isGameKilled)
            {
                await Task.Run(() => Utils.Cmd($"cd /d \"{Directory.GetParent(SettingsProvider.GameTexturesPath)?.Parent?.FullName}\" && timeout /t 1 && Audiosurf.exe"));
            }

            ApplicationNotificationManager.Manager.ShowSuccessWnd("Done!", "Operation completed!");
        }

        [RelayCommand]
        private void SaveCurrentFromGame()
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
                ApplicationNotificationManager.Manager.ShowErrorWnd("Error", $"Error while reading game configuration: {e.Message}");
                return;
            }

            var palette = presenter.ExportPalette();
            palette.Name = "My custom palette";
            Palettes.Add(palette);
            PaletteDynamicLoadContainer.Add(palette, PaletteContainerFilename);
            SelectPalette(palette);
        }

        private static async Task WaitForGameStopWorking(string procName)
        {
            while (Process.GetProcessesByName(procName).Length != 0)
                await Task.Delay(100);
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
                ApplicationNotificationManager.Manager.ShowErrorWnd("Error", "Error while exporting palette");
            else
                ApplicationNotificationManager.Manager.ShowSuccessWnd("Done!", "Operation completed!");
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
                ApplicationNotificationManager.Manager.ShowErrorWnd("Error", "Could not load the selected palette");
                return;
            }

            Palettes.Add(palette);
            PaletteDynamicLoadContainer.Add(palette, PaletteContainerFilename);
            ApplicationNotificationManager.Manager.ShowInformationWnd("", "Operation completed!");
            SelectPalette(palette);
        }
    }
}
