namespace SkinChangerRestyle.MVVM.ViewModel
{
    using ChangerAPI.Engine;
    using SkinChangerRestyle.Core;
    using System;
    using System.Windows;

    class SkinPreviewExtendedViewModel : ObservableObject
    {
        public RelayCommand BackToGridCommand { get; set; }

        public AudiosurfSkinExtended PinnedSkin { get; set; }

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
            SkinName = PinnedSkin == null ? "Empty Preview" : PinnedSkin.Name;
            if (StaticLink.GetObjectByTag(nameof(MainViewModel), out MainViewModel mvm))
            {
                BackToGridCommand = mvm.SetSkinsGridView;
            }

            else
            {
#if! DEBUG
                MessageBox.Show($"Initialization Fault: Can not get data model reference: {nameof(MainViewModel)}");
                Environment.Exit(0);
#endif
            }
        }
    }
}
