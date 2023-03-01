using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace SkinChangerRestyle.Core
{

    class ObservableObject : INotifyPropertyChanged
    {
        public bool ScrollAllowed { get; set; } = false;
        
        public event PropertyChangedEventHandler PropertyChanged;

        protected void OnPropertyChanged([CallerMemberName] string name = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
        }
    }
}
