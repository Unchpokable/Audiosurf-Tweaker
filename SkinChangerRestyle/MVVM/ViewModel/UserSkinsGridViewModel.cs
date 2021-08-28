namespace SkinChangerRestyle.MVVM.ViewModel
{
    using System;
    using System.Collections.Generic;
    using System.Linq;
    using System.Text;
    using System.Threading.Tasks;
    using SkinChangerRestyle.Core;

    class UserSkinsGridViewModel : ObservableObject
    {
        private object[] views;

        public object[] Views
        {
            get => views;
            set
            {
                if (value.Length != 6) throw new ArgumentException("views array must contains strictly 6 views");
                views = value;
                OnPropertyChanged();
            }
        }

        private MainViewModel mainVM;

        public UserSkinsGridViewModel()
        {
            StaticLink.RegisterObject(nameof(UserSkinsGridViewModel), this);
            StaticLink.GetObjectByTag(nameof(MainViewModel), out mainVM);

            views = new SkinPreviewViewModel[]
            {
                new SkinPreviewViewModel(),
                new SkinPreviewViewModel(),
                new SkinPreviewViewModel(),
                new SkinPreviewViewModel(),
                new SkinPreviewViewModel(),
                new SkinPreviewViewModel(),
                new SkinPreviewViewModel(),
                new SkinPreviewViewModel(),
                new SkinPreviewViewModel()
            };
        }
    }
}
