namespace SkinChangerRestyle.MVVM.ViewModel
{
    using System;
    using System.IO;
    using System.Windows.Media;
    using ChangerAPI.Engine;
    using SkinChangerRestyle.Core;
    using SkinChangerRestyle.MVVM.Model;
    using Env = SkinChangerRestyle.Core.EnvironmentContainer;

    class MainViewModel : ObservableObject
    {
        public RelayCommand SetSkinPreviewView { get; set; }
        public RelayCommand SetSkinsGridView { get; set; }
        public RelayCommand ConnectAudiosurfWindow { get; set; }

        public UserSkinsGridViewModel SkinsGridVM { get; set; }
        public SkinPreviewExtendedViewModel ExtendedSkinPreviewVM { get; set; }
        public object AudiosurfStatusMessage => asHandle?.StateMessage;
        public object AudiosurfStatusBackgroundColor => asHandle?.StateColor;

        private object currentView;
        private AudiosurfHandle asHandle;

        //private SkinEditorTool.SkinEditorController skinEditorController = new SkinEditorTool.SkinEditorController();

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
            InternalWorker.SetUpDefaultSettings();
            InternalWorker.InitializeEnvironment();
            asHandle = AudiosurfHandle.Instance;
            asHandle.StateChanged += OnASHandleStateChanged;
            SkinsGridVM = new UserSkinsGridViewModel();
            SetSkinsGridView = new RelayCommand(o => CurrentView = SkinsGridVM);
            ConnectAudiosurfWindow = new RelayCommand(o => asHandle.TryConnect());
        }

        private void OnASHandleStateChanged(object sender, EventArgs e)
        {
            OnPropertyChanged(nameof(AudiosurfStatusMessage));
            OnPropertyChanged(nameof(AudiosurfStatusBackgroundColor));
        }

        private void LoadSkins(string path)
        {
            var targets = Directory.EnumerateFiles(path);
            foreach (var target in targets)
            {
                AudiosurfSkinExtended skin = SkinPackager.Decompile(target);

                if (skin == null) continue;

                Env.LoadedSkins.Add(new SkinLink(target, skin));
            }
        }
    }
}
