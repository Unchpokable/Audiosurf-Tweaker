using SkinChangerRestyle.Core;
using SkinChangerRestyle.MVVM.Model;
using System.Collections.ObjectModel;
using System.Windows.Media;
using System.Linq;
using System.Windows;
using System.IO;
using IniParser;
using IniParser.Model;
using System.Diagnostics;
using System.Threading.Tasks;
using System;

namespace SkinChangerRestyle.MVVM.ViewModel
{
    internal class ColorsConfiguratorViewModel : ObservableObject
    {
        public ColorsConfiguratorViewModel()
        {
            var existsPalettes = PaletteDynamicLoadContainer.Load(PaletteContainerFilename);

            if (existsPalettes == null)
                existsPalettes = new PaletteDynamicLoadContainer();

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

            Palettes = new ObservableCollection<ColorPalette>(existsPalettes.ColorPalettes.Select(print => new ColorPalette(print)));
            PaletteDynamicLoadContainer.Save(existsPalettes, PaletteContainerFilename);

            RemoveSelectedPalette = new RelayCommand(RemoveSelectedPaletteInternal);
            DiscardChanges = new RelayCommand((o) => { SelectedPalette = new ColorPalette(_originPalette); OnPropertyChanged(nameof(SelectedPalette)); });
            ApplyChanges = new RelayCommand(ApplyChangesInternal);
            SaveAsNew = new RelayCommand(SaveAsNewInternal);
            WritePalette = new RelayCommand(WriteGameINI);
            ExportCurrentGameColorPalette = new RelayCommand(ExportGamePalette);
            ExportPalette = new RelayCommand(ExportPaletteInternal);
            ImportPalette = new RelayCommand(ImportPaletteInternal);
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

        public ColorPalette SelectedPalette
        {
            get => _selectedPalette;
            set
            {
                if (value == null) return;

                _originPalette = value;
                _selectedPalette = new ColorPalette(value);
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
                _selectedColor = value;

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

        private Color _selectedColor;
        private string _paletteName;
        private ObservableCollection<ColorPalette> _palettes;
        private ColorPalette _selectedPalette;
        private ColorPalette _originPalette;
        private ASColors _currentlyEditedColor;

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
                PaletteDynamicLoadContainer.Add(newPalette, PaletteContainerFilename);
            });
        }

        private void RemoveSelectedPaletteInternal(object o)
        {
            if (MessageBox.Show("This action will delete selected color palette. Are you sure?", "Remove palette", MessageBoxButton.YesNo, MessageBoxImage.Question) != MessageBoxResult.Yes)
                return;
            
            if (!Palettes.Contains(SelectedPalette))
                return;

            PaletteDynamicLoadContainer.Remove(SelectedPalette, PaletteContainerFilename);
            Palettes.Remove(SelectedPalette);
        }

        private async void WriteGameINI(object o)
        {
            var isGameKilled = false;
            var gamePid = Process.GetProcessesByName("QuestViewer");
            if (gamePid.Length > 0)
            {
                if (MessageBox.Show("Audiosurf Tweaker detected running game instance. Color settings can't be overwrited while game is running. Shutdown game to rewrite settings? This action will start your game back when operation complete", "Game is running", MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes)
                {
                    Core.Extensions.Extensions.Cmd($"taskkill /f /im \"{gamePid[0].ProcessName}.exe\"");
                    isGameKilled = true;
                    await WaitForGameStopWorking("QuestViewer");
                }
            }
            var pathToIni = Directory.GetParent(SettingsProvider.GameTexturesPath).FullName + "\\options.ini";
            var ini = new AudiosurfConfigurationPresenter();
            ini.ProcessConfigFile(pathToIni);
            ini.ApplyPalette(_selectedPalette);
            ini.SaveChanges();

            if (isGameKilled)
            {

                Core.Extensions.Extensions.Cmd($"cd /d \"{Directory.GetParent(SettingsProvider.GameTexturesPath).Parent.FullName}\" && timeout /t 1 && Audiosurf.exe");
            }

            MessageBox.Show("Done!", "Operation complete", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        private void ExportGamePalette(object obj)
        {
            var pathToIni = Directory.GetParent(SettingsProvider.GameTexturesPath).FullName + "\\options.ini";
            var ini = new AudiosurfConfigurationPresenter();
            try
            {
                ini.ProcessConfigFile(pathToIni);
            }
            catch
            {

            }

            var palette = ini.ExportPalette();
            palette.Name = "My custom palette";
            Palettes.Add(palette);
            SelectedPalette = palette;
            PaletteDynamicLoadContainer.Add(palette, PaletteContainerFilename);
        }

        private async Task WaitForGameStopWorking(string procName)
        {
            while (Process.GetProcessesByName(procName).Length != 0)
                await Task.Delay(100);
        }

        private async void ExportPaletteInternal(object o)
        {
            if (SelectedPalette == null)
                return;

            var sfDialog = new System.Windows.Forms.FolderBrowserDialog();
            if (sfDialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
            {
                await Task.Run(() => { ColorPalette.Save(SelectedPalette, sfDialog.SelectedPath); });
            }
        }

        private void ImportPaletteInternal(object o)
        {
            var fileDialog = new System.Windows.Forms.OpenFileDialog();
            if (fileDialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
            {
                var palette = ColorPalette.Load(fileDialog.FileName);
                if (palette == null)
                    MessageBox.Show("Could not load selected palette", "Import error", MessageBoxButton.OK, MessageBoxImage.Error);
                else
                {
                    Palettes.Add(palette);
                    PaletteDynamicLoadContainer.Add(palette, PaletteContainerFilename);
                }
            }
        }
    }
}
