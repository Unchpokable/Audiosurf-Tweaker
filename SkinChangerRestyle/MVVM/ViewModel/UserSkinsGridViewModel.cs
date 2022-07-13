namespace SkinChangerRestyle.MVVM.ViewModel
{
    using System;
    using System.Collections.Generic;
    using System.Collections.ObjectModel;
    using System.Linq;
    using System.Text;
    using System.Threading.Tasks;
    using ChangerAPI.Engine;
    using SkinChangerRestyle.Core;
    using SkinChangerRestyle.Core.Extensions;
    using SkinChangerRestyle.MVVM.Model;

    class UserSkinsGridViewModel : ObservableObject
    {

        public ObservableCollection<SkinCard> Skins { get; set; }    

        public UserSkinsGridViewModel()
        {
            StaticLink.RegisterObject(nameof(UserSkinsGridViewModel), this);
        }
    }
}
