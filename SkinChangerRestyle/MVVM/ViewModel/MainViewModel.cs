namespace SkinChangerRestyle.MVVM.ViewModel
{
    using System;
    using System.Collections.Generic;
    using System.Linq;
    using System.Text;
    using System.Threading.Tasks;
    using SkinChangerRestyle.Core;

    class MainViewModel : ObservableObject
    {
        public RelayCommand SetSkinPreviewView { get; set; }
        public RelayCommand SetSkinsGridView { get; set; }

        public UserSkinsGridViewModel SkinsGridVM { get; set; }
        public SkinPreviewExtendedViewModel ExtendedSkinPreviewVM { get; set; }


        private object currentView;

        public object CurrentView
        {
            get { return currentView; }
            set 
            {
                currentView = value;
                OnPropertyChanged();
            }
        }


        public MainViewModel()
        {
            SkinsGridVM = new UserSkinsGridViewModel();
            ExtendedSkinPreviewVM = new SkinPreviewExtendedViewModel();

            SetSkinPreviewView = new RelayCommand(o => CurrentView = ExtendedSkinPreviewVM);
            SetSkinsGridView = new RelayCommand(o => CurrentView = SkinsGridVM);
            CurrentView = ExtendedSkinPreviewVM;
        }

    }
}
