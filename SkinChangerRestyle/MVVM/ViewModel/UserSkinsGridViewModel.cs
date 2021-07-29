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

        public object FirstView
        {
            get { return view1; }
            set 
            {
                view1 = value;
                OnPropertyChanged();
            }
        }

    }
}
