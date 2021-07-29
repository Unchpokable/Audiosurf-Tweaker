namespace SkinChangerRestyle.MVVM.ViewModel
{
    using SkinChangerRestyle.Core;

    class SkinPreviewExtendedViewModel : ObservableObject
    {
        private object currImage;

        public object CurrentImage
        {
            get { return currImage; }
            set 
            {
                currImage = value;
                OnPropertyChanged();
            }
        }

    }
}
