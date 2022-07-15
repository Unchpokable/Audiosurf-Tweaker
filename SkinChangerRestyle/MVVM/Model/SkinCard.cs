using SkinChangerRestyle.Core;
using SkinChangerRestyle.Core.Extensions;
using SkinChangerRestyle.MVVM.ViewModel;
using System.Windows.Media;

namespace SkinChangerRestyle.MVVM.Model
{
    class SkinCard
    {
        public ImageSource Cover => pinnedSkin.Cover;
        public string Name => pinnedSkin.Name;

        public string InstallTooltip => "Install this skin fully";
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

        public string InstallTooltip => "Install this skin fully";
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

        private string _name;

        public DebugSkinCard(string name)
        {
            _name = name;
        }
    }
}
