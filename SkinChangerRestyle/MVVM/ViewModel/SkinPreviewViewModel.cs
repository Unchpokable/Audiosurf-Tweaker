namespace SkinChangerRestyle.MVVM.ViewModel
{
    using SkinChangerRestyle.Core;
    using System.Windows;
    using System.Windows.Input;
    using System.Windows.Media;

    class SkinPreviewViewModel : ObservableObject
    {
        private ImageSource coverImage;

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

        public SkinPreviewViewModel()
        {

            if (StaticLink.GetObjectByTag(nameof(MainViewModel), out MainViewModel mvmSource))
            {
                ShowExtendedPreviewCommand = mvmSource.SetSkinPreviewView;
            }
        }
    }
}
