using SkinChangerRestyle.Core;
using SkinChangerRestyle.Core.Extensions;
using SkinChangerRestyle.MVVM.Model;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Media;

namespace SkinChangerRestyle.MVVM.ViewModel
{
    internal class TweakerViewModel : ObservableObject
    {
        public bool IsAudiosurfConnected
        {
            get => _isAudiosurfConnected;
            set
            {
                _isAudiosurfConnected = value;
                OnPropertyChanged();
            }
        }

        public RelayCommand EnableTweak { get; private set; }
        public RelayCommand DisableTweak { get; private set; }
        public RelayCommand SendCommand { get; private set; }

        public ImageSource Cat => Properties.Resources.Cat.ToImageSource();

        private AudiosurfHandle _audiosurfHandle;
        private bool _isAudiosurfConnected;

        public TweakerViewModel()
        {
            _audiosurfHandle = AudiosurfHandle.Instance;
            _audiosurfHandle.StateChanged += (sender, e) => IsAudiosurfConnected = _audiosurfHandle.IsValid;

            EnableTweak = new RelayCommand(param =>
            {
                var fullMessage = $"asconfig {param}";
                _audiosurfHandle.Command(fullMessage);
            });

            DisableTweak = new RelayCommand(param =>
            {
                var fullMessage = $"asconfig {param}";
                _audiosurfHandle.Command(fullMessage);
            });

            SendCommand = new RelayCommand(param =>
            {
                _audiosurfHandle.Command($"ascommand {param}");
            });
        }
    }
}
