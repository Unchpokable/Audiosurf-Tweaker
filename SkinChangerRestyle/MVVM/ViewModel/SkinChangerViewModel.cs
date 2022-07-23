namespace SkinChangerRestyle.MVVM.ViewModel
{
    using System;
    using System.Collections.Generic;
    using System.Collections.ObjectModel;
    using System.Drawing;
    using System.Linq;
    using System.Text;
    using System.Threading.Tasks;
    using System.Windows.Media;
    using ChangerAPI.Engine;
    using SkinChangerRestyle;
    using SkinChangerRestyle.Core;
    using SkinChangerRestyle.Core.Extensions;
    using SkinChangerRestyle.MVVM.Model;

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
        public static SkinChangerViewModel Instance
        {
            get
            {
                if (_instance == null) _instance = new SkinChangerViewModel();
                return _instance;
            }
        }

        public ObservableCollection<DebugSkinCard> Skins { get; set; }    

        public DebugSkinCard SelectedItem
        { 
            get => _selectedItem;
            set
            {
                _selectedItem = value;
                OnPropertyChanged();
            }
        }

        public ImageSource InstallIcon { get; set; }
        public ImageSource ExportCopyIcon { get; set; }
        public ImageSource RenameIcon { get; set; }
        public ImageSource EditOnDiskIcon { get; set; }

        public RelayCommand ApartedInstallSelected;
        public RelayCommand TurnSkinPartInstallation;

        private DebugSkinCard _selectedItem;
        private static SkinChangerViewModel _instance;

        public SkinChangerViewModel()
        {

            InstallIcon = Properties.Resources.install.ToImageSource();
            ExportCopyIcon = Properties.Resources.export.ToImageSource();
            RenameIcon = Properties.Resources.edit.ToImageSource();
            EditOnDiskIcon = Properties.Resources.editondisk.ToImageSource();

            StaticLink.RegisterObject(nameof(SkinChangerViewModel), this);

            Skins = new ObservableCollection<DebugSkinCard>();

            for (int i = 0; i < 10; i++)
            {
                Skins.Add(new DebugSkinCard($"Debug skin {i}"));
            }
        }

        private void ClearInstall()
        {

        }

        private void ApartedInstall()
        {

        }
    }
}
