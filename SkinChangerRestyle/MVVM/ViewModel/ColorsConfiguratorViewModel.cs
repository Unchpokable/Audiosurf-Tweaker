using SkinChangerRestyle.Core;
using SkinChangerRestyle.MVVM.Model;
using System.Collections.ObjectModel;
using System.Windows.Media;
using System.Linq;
using System.Windows;
using System.IO;
using System.Threading.Tasks;
using System;
using Notification.Wpf;
using SkinChangerRestyle.Core.Extensions;
using SkinChangerRestyle.Core.Utils;

namespace SkinChangerRestyle.MVVM.ViewModel
{
    internal class ColorsConfiguratorViewModel : ObservableObject
    {
        protected ColorsConfiguratorViewModel()
        {
            var existsPalettes = PaletteDynamicLoadContainer.Load(PaletteContainerFilename);

            if (existsPalettes == null)
            {
                ApplicationNotificationManager.Manager.ShowOverWindow("Warning", 
                    "Palettes storage file was not found or corrupted and has been rewrited by new empty storage. Its OK if you running Audiosurf Tweaker for the first time",
                    NotificationType.Warning);
                existsPalettes = new PaletteDynamicLoadContainer();
            }

            if (existsPalettes.ColorPalettes.Count == 0)
                existsPalettes.Add(new ColorPalette() 
                { 
                    Name = "New Palette", 
                    Purple = Colors.Purple, 
                    Blue = Colors.Blue, 
                    Green = Colors.Lime, 
                    Yellow = Colors.Yellow, 
                    Red = Colors.Red 
                });

            Palettes = new ObservableCollection<ColorPalette>(existsPalettes.ColorPalettes.Select(print => new ColorPalette(print)).OrderBy(x => x.Name));
            PaletteDynamicLoadContainer.Save(existsPalettes, PaletteContainerFilename);
            EditedPalette = Palettes.FirstOrDefault();
            RemoveSelectedPalette = new RelayCommand(RemoveSelectedPaletteInternal);
            DiscardChanges = new RelayCommand((o) => { SelectedPalette = new ColorPalette(EditedPalette); OnPropertyChanged(nameof(SelectedPalette)); });
            ApplyChanges = new RelayCommand(ApplyChangesInternal);
            SaveAsNew = new RelayCommand(SaveAsNewInternal);
            WritePalette = new RelayCommand(WriteGameINI);
            ExportCurrentGameColorPalette = new RelayCommand(ExportGamePalette);
            ExportPalette = new RelayCommand(ExportPaletteInternal);
            ImportPalette = new RelayCommand(ImportPaletteInternal);
            MakeNegative = new RelayCommand(ConvertCurrentPaletteToNegative);
        }

        public static ColorsConfiguratorViewModel Instance
        {
            get
            {
                if (_instance == null)
                    _instance = new ColorsConfiguratorViewModel();
                return _instance;
            }
        }

        public static readonly string PaletteContainerFilename = "palettes";

        public RelayCommand ExportPalette { get; set; }
        public RelayCommand ImportPalette { get; set; }
        public RelayCommand ExportCurrentGameColorPalette { get; set; }
        public RelayCommand WritePalette { get; set; }
        public RelayCommand RemoveSelectedPalette { get; set; }
        public RelayCommand DiscardChanges { get; set; }
        public RelayCommand ApplyChanges { get; set; }
        public RelayCommand SaveAsNew { get; set; }

        public RelayCommand MakeNegative { get; set; }

        public ColorPalette EditedPalette
        {
            get => _originPalette;
            set
            {
                _originPalette = value;
                if (value == null)
                    SelectedPalette = value;
                else 
                    SelectedPalette = new ColorPalette(value);

                OnPropertyChanged();
            }
        }

        public ColorPalette SelectedPalette
        {
            get => _selectedPalette;
            set
            {
                if (value == null) return;

                _selectedPalette = value;
                CurrentlyEditedColor = ASColors.Purple;
                OnPropertyChanged();
                OnPropertyChanged(nameof(SelectedColor));
                OnPropertyChanged(nameof(SelectedColorBrush));
                OnPropertyChanged(nameof(PurpleBrush));
                OnPropertyChanged(nameof(BlueBrush));
                OnPropertyChanged(nameof(GreenBrush));
                OnPropertyChanged(nameof(YellowBrush));
                OnPropertyChanged(nameof(RedBrush));
            }
        }

        public Color SelectedColor
        {
            get
            {
                if (_selectedPalette == null)
                    return Colors.White;

                switch (CurrentlyEditedColor)
                {
                    case ASColors.Purple:
                        return _selectedPalette.Purple;
                    case ASColors.Blue:
                        return _selectedPalette.Blue;
                    case ASColors.Green:
                        return _selectedPalette.Green;
                    case ASColors.Yellow:
                        return _selectedPalette.Yellow;
                    case ASColors.Red:
                        return _selectedPalette.Red;
                    default:
                        return Colors.White;
                }
            }
            set 
            {
                switch (CurrentlyEditedColor)
                {
                    case ASColors.Purple:
                        _selectedPalette.Purple = value;
                        OnPropertyChanged(nameof(PurpleBrush));
                        break;
                    case ASColors.Blue:
                        _selectedPalette.Blue = value;
                        OnPropertyChanged(nameof(BlueBrush));
                        break;
                    case ASColors.Green:
                        _selectedPalette.Green = value;
                        OnPropertyChanged(nameof(GreenBrush));
                        break;
                    case ASColors.Yellow:
                        _selectedPalette.Yellow = value;
                        OnPropertyChanged(nameof(YellowBrush));
                        break;
                    case ASColors.Red:
                        _selectedPalette.Red = value;
                        OnPropertyChanged(nameof(RedBrush));
                        break;
                }

                OnPropertyChanged();
                OnPropertyChanged(nameof(SelectedColorBrush));
            }
        }

        public ASColors CurrentlyEditedColor
        {
            get => _currentlyEditedColor;
            set
            {
                _currentlyEditedColor = value;
                OnPropertyChanged(nameof(SelectedColor));
                OnPropertyChanged(nameof(SelectedColorBrush));
                OnPropertyChanged();
            }
        }

        public string PaletteName
        {
            get => _paletteName;
            set
            {
                _paletteName = value;
                OnPropertyChanged();
            }
        }

        public Brush PurpleBrush => _selectedPalette != null ? new SolidColorBrush(_selectedPalette.Purple) : null;
        public Brush BlueBrush => _selectedPalette != null ? new SolidColorBrush(_selectedPalette.Blue) : null;
        public Brush GreenBrush => _selectedPalette != null ? new SolidColorBrush(_selectedPalette.Green) : null;
        public Brush YellowBrush => _selectedPalette != null ? new SolidColorBrush(_selectedPalette.Yellow) : null;
        public Brush RedBrush => _selectedPalette != null ? new SolidColorBrush(_selectedPalette.Red) : null;

        public Brush SelectedColorBrush => new SolidColorBrush(SelectedColor);

        public ObservableCollection<ColorPalette> Palettes
        {
            get => _palettes;
            set
            {
                _palettes = value;
                OnPropertyChanged();
            }
        }

        private string _paletteName;
        private ObservableCollection<ColorPalette> _palettes;
        private ColorPalette _selectedPalette;
        private ColorPalette _originPalette;
        private ASColors _currentlyEditedColor;
        private static ColorsConfiguratorViewModel _instance;

        private async void ApplyChangesInternal(object o)
        {

            await Task.Run(() => 
            {
                PaletteDynamicLoadContainer.Replace(_originPalette, _selectedPalette, PaletteContainerFilename);
            });

            _originPalette.Purple = _selectedPalette.Purple;
            _originPalette.Blue = _selectedPalette.Blue;
            _originPalette.Green = _selectedPalette.Green;
            _originPalette.Yellow = _selectedPalette.Yellow;
            _originPalette.Red = _selectedPalette.Red; 
            _originPalette.Name = _selectedPalette.Name;
        }

        private async void SaveAsNewInternal(object o)
        {
            var newPalette = new ColorPalette(_selectedPalette);
            Palettes.Add(newPalette);
            await Task.Run(() =>
            {
                if (!PaletteDynamicLoadContainer.Add(newPalette, PaletteContainerFilename))
                {
                    MessageBox.Show("Error while writing cached palette storage. Changes will not be saved", "Cache error", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            });
        }

        private void RemoveSelectedPaletteInternal(object o)
        {
            if (!ApplicationNotificationManager.Manager.AskForAction("Remove Palette", "This action will delete selected color palette. Are you sure?"))
                return;

            if (!Palettes.Contains(SelectedPalette))
                return;

            if (!PaletteDynamicLoadContainer.Remove(SelectedPalette, PaletteContainerFilename))
            {
                MessageBox.Show("Error while writing cached palette storage. Changes will not be saved", "Cache error", MessageBoxButton.OK, MessageBoxImage.Error);
            }

            Palettes.Remove(SelectedPalette);
        }

        private async void WriteGameINI(object o)
        {
            var isGameKilled = false;
            var gamePid = System.Diagnostics.Process.GetProcessesByName("QuestViewer");
            if (gamePid.Length > 0)
            {
                if (ApplicationNotificationManager.Manager.AskForAction("Game is running", 
                        "Audiosurf Tweaker detected running game instance. Color settings can't be overwrited while game is running. Shutdown game to rewrite settings? This action will start your game back when operation complete"))
                {
                    Utils.Cmd($"taskkill /f /pid {gamePid[0].Id}");
                    isGameKilled = true;
                    await WaitForGameStopWorking("QuestViewer");
                }
                else
                {
                    return;
                }
            }

            var pathToIni = Directory.GetParent(SettingsProvider.GameTexturesPath)?.FullName + "\\options.ini";
            var ini = new AudiosurfConfigurationPresenter();

            try
            {
                ini.ProcessConfigFile(pathToIni);
                ini.ApplyPalette(_selectedPalette);
                ini.SaveChanges();
            }
            catch (Exception e)
            {
                ApplicationNotificationManager.Manager.ShowErrorWnd("Error", $"Error while writing game configuration file: {e.Message}");
            }

            if (isGameKilled)
            {
                await Task.Run(() => 
                {
                    Utils.Cmd($"cd /d \"{Directory.GetParent(SettingsProvider.GameTexturesPath)?.Parent?.FullName}\" && timeout /t 1 && Audiosurf.exe");
                });
            }

            ApplicationNotificationManager.Manager.ShowSuccessWnd("Done!", "Operation Completed!");
        }

        private void ExportGamePalette(object obj)
        {
            var pathToIni = Directory.GetParent(SettingsProvider.GameTexturesPath)?.FullName + "\\options.ini";
            var ini = new AudiosurfConfigurationPresenter();
            try
            {
                ini.ProcessConfigFile(pathToIni);
            }
            catch (Exception e)
            {
                ApplicationNotificationManager.Manager.ShowErrorWnd("Error", $"Error while reading/writing game configuration: {e.Message}");
                return;
            }

            var palette = ini.ExportPalette();
            palette.Name = "My custom palette";
            Palettes.Add(palette);
            SelectedPalette = palette;
            PaletteDynamicLoadContainer.Add(palette, PaletteContainerFilename);
        }

        private async Task WaitForGameStopWorking(string procName)
        {
            while (System.Diagnostics.Process.GetProcessesByName(procName).Length != 0)
                await Task.Delay(100);
        }

        private async void ExportPaletteInternal(object o)
        {
            if (SelectedPalette == null)
                return;

            var sfDialog = new System.Windows.Forms.FolderBrowserDialog();
            if (sfDialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
            {
                await Task.Run(() => 
                { 
                    if (!ColorPalette.Save(SelectedPalette, sfDialog.SelectedPath))
                    {
                        ApplicationNotificationManager.Manager.ShowErrorWnd("Error", "Error while exporting palette");
                    } 
                    else
                    {
                        ApplicationNotificationManager.Manager.ShowSuccessWnd("Done!", "Operation Completed!");
                    }
                });
            }
        }

        private void ImportPaletteInternal(object o)
        {
            var fileDialog = new System.Windows.Forms.OpenFileDialog();
            if (fileDialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
            {
                var palette = ColorPalette.Load(fileDialog.FileName);
                if (palette == null)
                    ApplicationNotificationManager.Manager.ShowErrorWnd("Error", "Could not load selected palette");
                else
                {
                    Palettes.Add(palette);
                    PaletteDynamicLoadContainer.Add(palette, PaletteContainerFilename);
                    ApplicationNotificationManager.Manager.ShowInformationWnd("", "Operation Completed!");
                }
            }
        }

        private void ConvertCurrentPaletteToNegative(object o)
        {
            _selectedPalette.Blue = _selectedPalette.Blue.ToNegative();
            _selectedPalette.Purple = _selectedPalette.Purple.ToNegative();
            _selectedPalette.Green = _selectedPalette.Green.ToNegative();
            _selectedPalette.Yellow = _selectedPalette.Yellow.ToNegative();
            _selectedPalette.Red = _selectedPalette.Red.ToNegative();

            OnPropertyChanged(nameof(PurpleBrush));
            OnPropertyChanged(nameof(BlueBrush));
            OnPropertyChanged(nameof(GreenBrush));
            OnPropertyChanged(nameof(YellowBrush));
            OnPropertyChanged(nameof(RedBrush));
        }
    }
}
