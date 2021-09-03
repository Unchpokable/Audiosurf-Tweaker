namespace SkinChangerRestyle.MVVM.ViewModel
{
    using SkinChangerRestyle.Core;

    class SkinPreviewExtendedViewModel : ObservableObject
    {
        public RelayCommand BackToGridCommand { get; set; }

        private string skinName;

        public string SkinName
        {
            get => skinName;
            set
            {
                skinName = value;
                OnPropertyChanged();
            }
        }
        public SkinPreviewExtendedViewModel()
        {
            SkinName = "Empty Preview";
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
