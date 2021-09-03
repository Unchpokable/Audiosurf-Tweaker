namespace SkinChangerRestyle.MVVM.ViewModel
{
    using System;
    using System.Collections.Generic;
    using System.IO;
    using System.Linq;
    using System.Text;
    using System.Threading.Tasks;
    using ChangerAPI.Engine;
    using SkinChangerRestyle.Core;
    using Env = SkinChangerRestyle.Core.EnvironmentContainer;

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
            StaticLink.RegisterObject(nameof(MainViewModel), this);

            SkinsGridVM = new UserSkinsGridViewModel();
            ExtendedSkinPreviewVM = new SkinPreviewExtendedViewModel();

            SetSkinPreviewView = new RelayCommand(o => CurrentView = ExtendedSkinPreviewVM);
            SetSkinsGridView = new RelayCommand(o => CurrentView = SkinsGridVM);

            CurrentView = ExtendedSkinPreviewVM;
        }

        private void LoadSkins(string path)
        {
            var targets = Directory.EnumerateFiles(path);
            foreach (var target in targets)
            {
                AudiosurfSkinExtended skin = SkinPackager.Decompile(target);

                if (skin == null) continue;

                Env.LoadedSkins.Add(new SkinLink(target, skin.Name));
            }
        }
    }
}
