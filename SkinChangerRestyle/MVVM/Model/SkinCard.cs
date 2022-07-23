using SkinChangerRestyle.Core;
using SkinChangerRestyle.Core.Extensions;
using SkinChangerRestyle.MVVM.ViewModel;
using System.Collections.ObjectModel;
using System.Linq;
using System.Windows.Media;

namespace SkinChangerRestyle.MVVM.Model
{
    class SkinCard
    {
        public ImageSource Cover => pinnedSkin.Cover;
        public string Name => pinnedSkin.Name;

        public string InstallTooltip => "Clear Installation";
        public string ExportCopyTooltip => "Export copy of this skin";
        public string RenameTooltip => "Rename this skin";
        public string EditOnDiskTooltip => "Turn ASTweaker into EditOnDisk mode";

        public ImageSource InstallIcon => Properties.Resources.install.ToImageSource();
        public ImageSource ExportCopyIcon => Properties.Resources.export.ToImageSource();
        public ImageSource RenameIcon => Properties.Resources.edit.ToImageSource();
        public ImageSource EditOnDiskIcon => Properties.Resources.editondisk.ToImageSource();

        public RelayCommand InstallCommand { get; set; }
        public RelayCommand ExportCopyCommand { get; set; }
        public RelayCommand RenameCommand { get; set; }
        public RelayCommand EditOnDiskCommand { get; set; }

        private SkinLink pinnedSkin;
        private SkinChangerViewModel rootVM;

        public SkinCard(SkinLink skin, SkinChangerViewModel root)
        {
            pinnedSkin = skin;
            rootVM = root;
        }
    }

    internal class DebugSkinCard
    {
        public string Name => _name;

        public string InstallTooltip => "Full texture set replacement with clear installation";
        public string ExportCopyTooltip => "Export copy of this skin";
        public string RenameTooltip => "Rename this skin";
        public string EditOnDiskTooltip => "Edit this skin on disk";

        public ImageSource InstallIcon { get; set; }
        public ImageSource ExportCopyIcon { get; set; }
        public ImageSource RenameIcon { get; set; }
        public ImageSource EditOnDiskIcon { get; set; }
        public ImageSource Cover { get; set; }

        public RelayCommand InstallCommand { get; set; }
        public RelayCommand ExportCopyCommand { get; set; }
        public RelayCommand RenameCommand { get; set; }
        public RelayCommand EditOnDiskCommand { get; set; }

        public ObservableCollection<InteractableScreenshot> Screenshots { get; set; }

        private string _name;

        public DebugSkinCard(string name)
        {
            _name = name;
            Screenshots = new ObservableCollection<InteractableScreenshot>();
            for (int i = 0; i < 10; i++)
            {
                Screenshots.Add(new InteractableScreenshot(Properties.Resources.Pintman.ToImageSource()));
            }

            InstallIcon = Properties.Resources.install.ToImageSource();
            ExportCopyIcon = Properties.Resources.export.ToImageSource();
            RenameIcon = Properties.Resources.edit.ToImageSource();
            EditOnDiskIcon = Properties.Resources.editondisk.ToImageSource();
            Cover = Screenshots?.First().Image;
        }
    }
}
