namespace SkinChangerRestyle.MVVM.ViewModel
{
    using SkinChangerRestyle.Core;

    class SkinPreviewExtendedViewModel : ObservableObject
    {
        public RelayCommand BackToGridCommand { get; set; }

        public SkinPreviewExtendedViewModel()
        {
            if (StaticLink.GetObjectByTag(nameof(MainViewModel), out MainViewModel mvm))
            {
                BackToGridCommand = mvm.SetSkinsGridView;
            }
            else
            {
                BackToGridCommand = new RelayCommand(o => { }, o => false);
            }
        }
    }
}
