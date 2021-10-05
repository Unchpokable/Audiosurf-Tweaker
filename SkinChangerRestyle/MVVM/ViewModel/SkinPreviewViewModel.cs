namespace SkinChangerRestyle.MVVM.ViewModel
{
    using System;
    using SkinChangerRestyle.Core;
    using SkinChangerRestyle.Core.Extensions;
    using System.Windows;
    using System.Windows.Input;
    using System.Windows.Media;
    using ChangerAPI.Engine;
    using System.Drawing;
    using SkinChangerRestyle.Properties;

    class SkinPreviewViewModel : ObservableObject
    {
        public AudiosurfSkinExtended PinnedSkin
        {
            get => pinnedSkin;
            set
            {
                pinnedSkin = value;
                if ((Bitmap)pinnedSkin.Cover == null)
                    CoverImage = Resources.MissingTextureFullHD.ToImageSource();
                CoverImage = ((Bitmap)pinnedSkin.Cover).ToImageSource();
            }
        }

        public ImageSource CoverImage
        {
            get => coverImage;
            set
            {
                coverImage = value;
                OnPropertyChanged();
            }
        }

        public ICommand ShowExtendedPreviewCommand { get; set; }

        public RelayCommand LoadExtendedPreviewData { get; set; }

        private AudiosurfSkinExtended pinnedSkin;
        private ImageSource coverImage;

        public SkinPreviewViewModel()
        {
            if (StaticLink.GetObjectByTag(nameof(MainViewModel), out MainViewModel mvmSource) && StaticLink.GetObjectByTag(nameof(UserSkinsGridViewModel), out UserSkinsGridViewModel usgridVMSource))
            {
                ShowExtendedPreviewCommand = mvmSource.SetSkinPreviewView;

            }
            else
            {
#if !DEBUG
                MessageBox.Show($"Initialization Fault: Can not get data model reference: {nameof(MainViewModel)}");
                Environment.Exit(0);
#endif
            }
        }
    }
}
