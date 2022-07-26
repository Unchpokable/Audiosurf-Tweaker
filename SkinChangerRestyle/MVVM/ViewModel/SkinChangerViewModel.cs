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

    enum SkinPart
    {
        Skyspheres,
        Tileset,
        Particles,
        Rings,
        Hits
    }

    class SkinChangerViewModel : ObservableObject
    {
        protected SkinChangerViewModel()
        {

            InstallIcon = Properties.Resources.install.ToImageSource();
            ExportCopyIcon = Properties.Resources.export.ToImageSource();
            RenameIcon = Properties.Resources.edit.ToImageSource();
            EditOnDiskIcon = Properties.Resources.editondisk.ToImageSource();

            AddNewSkin = new RelayCommand(AddNewSkinInternal);
            ExportCurrentTextures = new RelayCommand(ExportCurrentTexturesInternal);

            Skins = new ObservableCollection<SkinCard>();

            LoadSkins();

            InstallSelected = new RelayCommand((param) =>
            {
                InstallSelectedAsync();
            });

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
        
        public SkinCard SelectedItem
        {
            get => _selectedItem;
            set
            {
                _selectedItem = value;
                OnPropertyChanged();
            }
        }

        public ObservableCollection<SkinCard> Skins { get; set; }     

        public ImageSource InstallIcon { get; set; }
        public ImageSource ExportCopyIcon { get; set; }
        public ImageSource RenameIcon { get; set; }
        public ImageSource EditOnDiskIcon { get; set; }

        public RelayCommand InstallSelected { get; set; }
        public RelayCommand InstallFull { get; set; }
        public RelayCommand AddNewSkin { get; set; }
        public RelayCommand ExportCurrentTextures { get; set; }

        public bool ShouldInstallSkyshpheres { get; set; }
        public bool ShouldInstallTileset { get; set; }
        public bool ShouldInstallParticles { get; set; }
        public bool ShouldInstallRings { get; set; }
        public bool ShouldInstallHits { get; set; }


        private SkinCard _selectedItem;
        private static SkinChangerViewModel _instance;
        private object _lockObject = new object();

        public Task InstallSkin(string pathToOrigin, string target, bool forced = false, bool unpackScreenshots = false)
        {
            return Task.Run(() =>
            {
                var skin = SkinPackager.Decompile(pathToOrigin);
                if (skin == null)
                    return;

                if (ShouldInstallSkyshpheres || forced)
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
            });
        }

        private async void InstallSelectedAsync()
        {
            await InstallSkin(_selectedItem.PathToOrigin, SettingsProvider.GameTexturesPath);
        }

        private void AddNewSkinInternal(object frameworkReqieredParameter)
        {
            var path = new System.Windows.Forms.OpenFileDialog();
            path.InitialDirectory = Environment.GetFolderPath(Environment.SpecialFolder.Personal) + @"\Downloads";
            path.Filter = "Tweaker Skin Package|*.askin2";

            if (path.ShowDialog() == System.Windows.Forms.DialogResult.OK)
            {
                var card = new SkinCard(SkinPackager.Decompile(path.FileName), this);
                Skins.Add(card);
            }
        }

        private void LoadSkins()
        {
            var files = Directory.EnumerateFiles(@"Skins");
            if (Directory.Exists(SettingsProvider.SkinsFolderPath))
                files = files.Concat(Directory.EnumerateFiles(SettingsProvider.SkinsFolderPath));

            foreach (var file in files)
            {
                var skin = SkinPackager.Decompile(file);
                if (skin == null) continue;

                var card = new SkinCard(skin, this);
                Skins.Add(card);
            }
        }

        private void ExportCurrentTexturesInternal(object frameworkRequeredParameter)
        {
            var skin = SkinPackager.CreateSkinFromFolder(SettingsProvider.GameTexturesPath);
            skin.Name = "New exported skin";
            skin.Source = $@"Skins\{skin.Name}.askin2";
            var card = new SkinCard(skin, this);
            Skins.Add(card);
            SkinPackager.CompileTo(skin, "Skins");   
        }
    }
}
