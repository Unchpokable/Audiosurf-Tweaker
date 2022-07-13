using SkinChangerRestyle.Core;
using System.Windows.Media;

namespace SkinChangerRestyle.MVVM.Model
{
    class SkinCard
    {
        public ImageSource Cover => pinnedSkin.Cover;
        public string Name => pinnedSkin.Name;
        public RelayCommand ShowInstallationDetails { get; set; }

        private SkinLink pinnedSkin;

        public SkinCard(SkinLink skin)
        {
            pinnedSkin = skin;
            ShowInstallationDetails = new RelayCommand((o) => {
            });
        }
    }
}
