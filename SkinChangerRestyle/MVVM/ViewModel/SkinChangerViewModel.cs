namespace SkinChangerRestyle.MVVM.ViewModel
{
    using System;
    using System.Collections.Generic;
    using System.Collections.ObjectModel;
    using System.Drawing;
    using System.IO;
    using System.Linq;
    using System.Text;
    using System.Threading.Tasks;
    using System.Windows.Media;
    using ChangerAPI.Engine;
    using SkinChangerRestyle;
    using SkinChangerRestyle.Core;
    using SkinChangerRestyle.Core.Extensions;
    using SkinChangerRestyle.MVVM.Model;
    using System.Windows.Data;
    using FolderChecker;
    using System.Windows.Forms;
    using System.Collections.Concurrent;
    using System.Threading;

    class SkinChangerViewModel : ObservableObject
    {
        protected SkinChangerViewModel()
        {
            ShouldInstallSkyspheres = true;
            ShouldInstallTileset = true;
            ShouldInstallRings = true;
            ShouldInstallParticles = true;
            ShouldInstallHits = true;
            ReloadButtonUnlocked = true;

            AddNewSkin = new RelayCommand(AddNewSkinInternal);
            ExportCurrentTextures = new RelayCommand(ExportCurrentTexturesInternal);
            InstallSelectedCommand = new RelayCommand(InstallSelected);
            RemoveSelected = new RelayCommand(o => RemoveSkin(SelectedItem));

            AddNewIcon = Properties.Resources.plus.ToImageSource();
            ExportMyTexturesIcon = Properties.Resources.exportmy.ToImageSource();
            RefreshIcon = Properties.Resources.refreshing.ToImageSource();

            Skins = new ObservableCollection<SkinCard>();

            ReloadSkins = new RelayCommand((param) =>
            {
                ReloadButtonUnlocked = false;
                Skins.Clear();
                LoadSkins();
            });

            LoadSkins();
            //LoadSkinParallel();

            if (EnvironmentChecker.CheckEnvironment(SettingsProvider.GameTexturesPath, out FolderHashInfo state))
                CurrentInstalledSkin = state.StateName;
            else 
                CurrentInstalledSkin = null;

            BindingOperations.EnableCollectionSynchronization(Skins, _lockObject);
        }

        public static SkinChangerViewModel Instance
        {
            get
            {
                if (_instance == null) _instance = new SkinChangerViewModel();
                return _instance;
            }
        }


        private System.Windows.Visibility _loadingProgressbarVisible;

        public System.Windows.Visibility LoadingProgressbarVisible
        {
            get => _loadingProgressbarVisible;
            set 
            { 
                _loadingProgressbarVisible = value;
                OnPropertyChanged();
            }
        }


        public int TotalSkinsToLoad
        {
            get => _totalSkinsCount;
            set
            {
                _totalSkinsCount = value;
                OnPropertyChanged();
            }
        }

        public int CurrentLoadStep
        {
            get => _currentLoadStep; 
            set 
            { 
                _currentLoadStep = value; 
                OnPropertyChanged(); 
            }
        }


        private bool _reloadButtonLocked;
        public bool ReloadButtonUnlocked
        {
            get => _reloadButtonLocked;
            set
            {
                _reloadButtonLocked = value;
                OnPropertyChanged();
            }
        }

        public SkinCard SelectedItem
        {
            get => _selectedItem;
            set
            {
                _selectedItem = value;
                OnPropertyChanged();
            }
        }

        public string CurrentInstalledSkin
        {
            get => _currentSkinName == null ? "Unsaved" : _currentSkinName;
            set
            {
                _currentSkinName = value;
                OnPropertyChanged();
            }
        }

        public bool ShouldInstallSkyspheres
        {   
            get => _shouldInstallSkyspheres;
            set
            {
                _shouldInstallSkyspheres = value;
                OnPropertyChanged();
            }
        }

        public bool ShouldInstallTileset
        {
            get => _shouldInstallTileset;
            set
            {
                _shouldInstallTileset = value;
                OnPropertyChanged();
            }
        }

        public bool ShouldInstallParticles
        {
            get => _shouldInstallParticles;
            set
            {
                _shouldInstallParticles = value;
                OnPropertyChanged();
            }
        }

        public bool ShouldInstallRings
        {
            get => _shouldInstallRings;
            set
            {
                _shouldInstallRings = value;
                OnPropertyChanged();
            }
        }

        public bool ShouldInstallHits
        {
            get => _shouldInstallHits;
            set
            {
                _shouldInstallHits = value;
                OnPropertyChanged();
            }
        }

        public ObservableCollection<SkinCard> Skins
        {
            get => _skins;
            set
            {
                _skins = value;
                OnPropertyChanged();
                BindingOperations.EnableCollectionSynchronization(_skins, _lockObject);
            }
        }

        private ObservableCollection<int> _test;

        public ObservableCollection<int> Test
        {
            get { return _test; }
            set { _test = value; OnPropertyChanged(); }
        }

        public ImageSource AddNewIcon { get; set; }
        public ImageSource ExportMyTexturesIcon { get; set; }
        public ImageSource RefreshIcon { get; set; }

        public RelayCommand InstallSelectedCommand { get; set; }
        public RelayCommand InstallFullCommand { get; set; }
        public RelayCommand AddNewSkin { get; set; }
        public RelayCommand ExportCurrentTextures { get; set; }
        public RelayCommand ReloadSkins { get; set; }
        public RelayCommand RemoveSelected { get; set; }

        private bool _shouldInstallSkyspheres;
        private bool _shouldInstallTileset;
        private bool _shouldInstallParticles;
        private bool _shouldInstallRings;
        private bool _shouldInstallHits;

        private ObservableCollection<SkinCard> _skins;
        private SkinCard _selectedItem;
        private static SkinChangerViewModel _instance;
        private string _currentSkinName;
        private object _lockObject = new object();
        private int _currentLoadStep;
        private int _totalSkinsCount;

        public async void InstallSkin(string pathToOrigin, string target, bool forced = false, bool unpackScreenshots = false, bool clearInstall = false, bool saveState = true)
        {
            if (target.Equals(SettingsProvider.GameTexturesPath, StringComparison.InvariantCultureIgnoreCase))
            {
                if (SettingsProvider.SafeInstall)
                {
                    if (!EnvironmentChecker.CheckEnvironment(target, out FolderHashInfo _))
                    {
                        MessageBox.Show("Current texutre set in unsaved. Skin installation prohibited", "Warning", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        return;
                    }
                }

                if (!EnvironmentChecker.CheckEnvironment(target, out FolderHashInfo _) && SettingsProvider.ControlSystemActive)
                {
                    var userReply = MessageBox.Show("Your current texture set is unsaved. Installing skin will overwrite unsaved changes and you will lost it. Do you want to continue?", "Warning",
                        MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
                    if (userReply == DialogResult.No)
                        return;
                }
            }

            if (clearInstall)
            {
                Clean(target);
                //await InstallSkinInternal(@"Skins\Default.askin2", target, forced: true, saveState: false);
                AudiosurfHandle.Instance.Command("ascommand reloadtextures");
            }
            await InstallSkinInternal(pathToOrigin, target, forced: forced, unpackScreenshots: unpackScreenshots, saveState: saveState);

            if (SettingsProvider.HotReload)
                AudiosurfHandle.Instance.Command("ascommand reloadtextures");
        }

        public async void RemoveSkin(SkinCard target)
        {
            if (!Skins.Contains(target))
                return;

            Skins.Remove(target);

            if (MessageBox.Show("Remove file too?", "removing skin", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
            {
                await Task.Run(() =>
                {
                    File.Delete(target.PathToOrigin);
                });
            }
        }

        private Task InstallSkinInternal(string pathToOrigin, string target,
                                         bool forced = false,
                                         bool unpackScreenshots = false,
                                         bool saveState = false)
        {
            return Task.Run(() =>
            {
                var skin = SkinPackager.Decompile(pathToOrigin);
                if (skin == null)
                    return;

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
                    skin.Previews.Apply(x => x?.Save($@"{target}\Screenshots"));
                }

                if (saveState)
                {
                    var state = FolderHashInfo.Create(target, skin.Name);
                    state.Save(target);
                    CurrentInstalledSkin = state.StateName;
                }

                if (SettingsProvider.HotReload)
                    AudiosurfHandle.Instance.Command("ascommand reloadtextures");
                skin.Dispose();
                GC.Collect();
            });
        }

        private void InstallSelected(object freameworkRequieredParameter)
        {
            if (_selectedItem == null)
                return;

            InstallSkin(_selectedItem.PathToOrigin, SettingsProvider.GameTexturesPath);
        }


        private void AddNewSkinInternal(object frameworkReqieredParameter)
        {
            var path = new OpenFileDialog();
            path.InitialDirectory = Environment.GetFolderPath(Environment.SpecialFolder.Personal) + @"\Downloads";
            path.Filter = "Tweaker Skin Package|*.askin2";

            if (path.ShowDialog() == DialogResult.OK)
            {
                var skin = SkinPackager.Decompile(path.FileName);
                if (skin == null)
                {
                    MessageBox.Show("Selected file isn't Audiosurf Tweaker package", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }
                var card = new SkinCard(skin, path.FileName, this);
                Skins.Add(card);
            }
        }

        private void LoadSkins()
        {
            var files = Directory.EnumerateFiles(@"Skins").ToList();
            if (Directory.Exists(SettingsProvider.SkinsFolderPath))
                files.AddRange(Directory.EnumerateFiles(SettingsProvider.SkinsFolderPath));

            TotalSkinsToLoad = files.Count;
            CurrentLoadStep = 0;
            LoadingProgressbarVisible = System.Windows.Visibility.Visible;
            Task.Factory.StartNew(() =>
            {
                foreach (var file in files)
                {
                    var skin = SkinPackager.Decompile(file);
                    MainWindow.WindowDispatcher.Invoke(new Action(() =>
                    {
                        if (skin == null) return;
                        var card = new SkinCard(skin, file, this);
                        Skins.Add(card);
                        skin.Dispose();
                    }));
                    CurrentLoadStep++;
                }
                After(1000, () =>
                {
                    ReloadButtonUnlocked = true;
                    GC.Collect();
                    // Ye, i know that manual calling GC.Collct() is a very bad practice, but idk why, in this certain case GC works as shit bag and lefts OVER NINE THOUSANDS unsed memory for an undefined long while
                });
                LoadingProgressbarVisible = System.Windows.Visibility.Hidden;
            });
        }

        private async void ExportCurrentTexturesInternal(object frameworkRequeredParameter)
        {
            var skin = SkinPackager.CreateSkinFromFolder(SettingsProvider.GameTexturesPath);
            skin.Name = "New exported skin";
            skin.Source = $@"Skins\{skin.Name}.askin2";
            var card = new SkinCard(skin, skin.Source, this);
            if (!Skins.Any(c => c.PathToOrigin.Equals(card.PathToOrigin, StringComparison.OrdinalIgnoreCase)))
                Skins.Add(card);
            else
            {
                Skins.Remove(Skins.First(c => c.PathToOrigin.Equals(card.PathToOrigin, StringComparison.OrdinalIgnoreCase)));
                Skins.Add(card);
            }
            card.EnableRename.Execute(new object());
            await Task.Run(() => { SkinPackager.CompileTo(skin, "Skins"); skin.Dispose(); });
            SelectedItem = card;
            After(1000, () => GC.Collect());
        }

        private void Clean(string path)
        {
            DirectoryInfo di = new DirectoryInfo(path);

            foreach (FileInfo file in di.EnumerateFiles())
            {
                file.Delete();
            }
            foreach (DirectoryInfo dir in di.EnumerateDirectories())
            {
                dir.Delete(true);
            }
        }

        private void After(int msec, Action command)
        {
            Task.Factory.StartNew(() =>
            {
                Thread.Sleep(msec);
                command?.Invoke();
            });
        }
    }
}
