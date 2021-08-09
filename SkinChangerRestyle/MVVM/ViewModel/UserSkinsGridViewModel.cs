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
        private object view1;
        private object view2;
        private object view3;
        private object view4;
        private object view5;
        private object view6;


        public object View1
        {
            get { return view1; }
            set 
            {
                view1 = value;
                OnPropertyChanged();
            }
        }

        public object View2
        {
            get { return view2; }
            set
            {
                view2 = value;
                OnPropertyChanged();
            }
        }

        public object View3
        {
            get { return view3; }
            set
            {
                view3 = value;
                OnPropertyChanged();
            }
        }

        public object View4
        {
            get { return view4; }
            set
            {
                view4 = value;
                OnPropertyChanged();
            }
        }

        public object View5
        {
            get { return view5; }
            set
            {
                view5 = value;
                OnPropertyChanged();
            }
        }

        public object View6
        {
            get { return view6; }
            set
            {
                view6 = value;
                OnPropertyChanged();
            }
        }


        public UserSkinsGridViewModel()
        {
            StaticLink.RegisterObject(nameof(UserSkinsGridViewModel), this);
        }
    }
}
