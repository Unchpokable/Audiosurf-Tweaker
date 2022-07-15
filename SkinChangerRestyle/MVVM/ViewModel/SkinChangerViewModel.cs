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

    class SkinChangerViewModel : ObservableObject
    {

        public ObservableCollection<DebugSkinCard> Skins { get; set; }    

        public ImageSource TestButtonIcon => new Bitmap(Properties.Resources.testicon).ToImageSource();

        public ImageSource InstallIcon => Properties.Resources.install.ToImageSource();
        public ImageSource ExportCopyIcon => Properties.Resources.export.ToImageSource();
        public ImageSource RenameIcon => Properties.Resources.edit.ToImageSource();
        public ImageSource EditOnDiskIcon => Properties.Resources.editondisk.ToImageSource();

        public SkinChangerViewModel()
        {
            StaticLink.RegisterObject(nameof(SkinChangerViewModel), this);

            Skins = new ObservableCollection<DebugSkinCard>();

            for (int i = 0; i < 10; i++)
            {
                Skins.Add(new DebugSkinCard($"Debug skin {i}"));
            }
        }
    }
}
