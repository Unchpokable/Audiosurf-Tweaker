namespace SkinChangerRestyle.MVVM.ViewModel
{
    using System;
    using System.Collections.Generic;
    using System.Linq;
    using System.Text;
    using System.Threading.Tasks;
    using ChangerAPI.Engine;
    using SkinChangerRestyle.Core;
    using SkinChangerRestyle.Core.Extensions;
    using SkinChangerRestyle.MVVM.Model;

    class UserSkinsGridViewModel : ObservableObject
    {

        public string CurrentPage
        {
            get => currPage.ToString();
            set
            {
                if (int.TryParse(value, out int page) && page > 0 && page < maxPages)
                {
                    currPage = page;
                    OnPropertyChanged();
                    AssignSkinsToPreviews();
                }
            }
        }

        public string MaxPages
        {
            get => maxPages.ToString();
            set
            {
                if (int.TryParse(value, out int pageCount))
                {
                    maxPages = pageCount;
                    OnPropertyChanged();
                    //AssignSkinsToPreviews();
                }
            }
        }

        public object[] Views
        {
            get => views;
            set
            {
                if (value.Length != 9) throw new ArgumentException("views array must contains strictly 9 views");
                views = value;
                OnPropertyChanged();
            }
        }

        public Queue<SkinLink> FreeSkins { get; private set; }
        public RelayCommand ChangePageCommand { get; set; } 

        private List<Queue<SkinLink>> AllLoadedSkins;
        private MainViewModel mainVM;
        private object[] views;
        private int currPage;
        private int maxPages;

        public UserSkinsGridViewModel()
        {
            StaticLink.RegisterObject(nameof(UserSkinsGridViewModel), this);
            StaticLink.GetObjectByTag(nameof(MainViewModel), out mainVM);

            AllLoadedSkins = EnvironmentContainer.LoadedSkinsByChunks?.Select(skinGroup => skinGroup.ToQueue()).ToList();
            maxPages = AllLoadedSkins.Count / EnvironmentContainer.ChunkSize;

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

            ChangePageCommand = new RelayCommand((direction) =>
            {
                PageChangingDirection changeWay;
                try
                {
                    changeWay = (PageChangingDirection)direction;
                }
                catch
                {
                    throw new ArgumentException($"Parameter {direction} isn't {nameof(PageChangingDirection)} but should be");
                }

                switch (changeWay)
                {
                    case PageChangingDirection.Next:
                        currPage++;
                        break;
                    case PageChangingDirection.Previous:
                        if (currPage > 1)
                        {
                            currPage--;
                            break;
                        }
                        break;
                }
                CurrentPage = currPage.ToString();
            });
        }

        private void AssignSkinsToPreviews()
        {
            if (AllLoadedSkins.Count == 0) return;

            var currPageQueue = new Queue<SkinLink>(AllLoadedSkins[currPage - 1]);

            foreach (var view in views)
            {
                var vm = (SkinPreviewViewModel)view;
                vm.PinnedSkin = SkinPackager.Decompile(currPageQueue.Dequeue().Path);
            }
        }
    }
}
