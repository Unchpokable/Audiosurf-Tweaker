using SkinChangerRestyle.Core;
using SkinChangerRestyle.Core.Extensions;
using System.Windows.Media;
using ASCommander;
using System.Text;
using System;

namespace SkinChangerRestyle.MVVM.ViewModel
{
    internal class TweakerViewModel : ObservableObject
    {
        public TweakerViewModel()
        {
            _audiosurfHandle = AudiosurfHandle.Instance;
            _audiosurfHandle.StateChanged += (sender, e) => IsAudiosurfConnected = _audiosurfHandle.IsValid;
            _console = new TweakerConsole();
            _console.ContentUpdated += (s, e) => OnPropertyChanged(nameof(ConsoleContent));

            SendCommand = new RelayCommand(param =>
            {
                _audiosurfHandle.Command($"ascommand {param}");
            });

            FlushConsole = new RelayCommand(param =>
            {
                _console.Flush();
                OnPropertyChanged(nameof(ConsoleContent));
            });
        }

        ~TweakerViewModel()
        {
            // Setters of this fields instantly sends asconfing command to game. So, program turning off must rollback all undefault game's configuration
            // this desctuctor should be called before AudiosurfHandle destructor cause TweakerViewModel has a reference to AudiosurfHandle so this should work
            InvisibleRoadTweakActive = false;
            HiddenSongTweakActive = false;
            SidewinderCameraTweakActive = false;
            BankingCameraTweakActive = false;
            FreerideAutoAdvanceDisableTweakActive = false;
            FreerideBlocksCaterpillarsTweakActive = false;
            FreerideNoBlocksTweakActive = false;
        }

        public bool IsAudiosurfConnected
        {
            get => _isAudiosurfConnected;
            set
            {
                _isAudiosurfConnected = value;
                OnPropertyChanged();
            }
        }

        public bool InvisibleRoadTweakActive
        {
            get => _invisibleRoadTweakActive; 
            set
            {
                _invisibleRoadTweakActive = value;
                _audiosurfHandle.Command($"asconfig roadvisible {(!value).ToString().ToLower()}");
                OnPropertyChanged();
            }
        }

        public bool HiddenSongTweakActive
        {
            get => _hiddenSongNameTweakActive; 
            set 
            { 
                _hiddenSongNameTweakActive = value;
                _audiosurfHandle.Command($"asconfig showsongname {(!value).ToString().ToLower()}");
                OnPropertyChanged();
            }
        }

        public bool SidewinderCameraTweakActive
        {
            get => _sidewinderCameraTweakActive;
            set
            {
                _sidewinderCameraTweakActive = value;
                _audiosurfHandle.Command($"asconfig sidewinder {value.ToString().ToLower()}");
                OnPropertyChanged();
            }
        }

        public bool BankingCameraTweakActive
        {
            get => _bankingCameraTweakActive;
            set
            {
                _bankingCameraTweakActive = value;
                _audiosurfHandle.Command($"asconfig usebankingcamera {value.ToString().ToLower()}");
                OnPropertyChanged();
            }
        }


        public bool FreerideNoBlocksTweakActive
        {
            get => _freerideNoBlocksTweakActive;
            set 
            { 
                _freerideNoBlocksTweakActive = value;
                _audiosurfHandle.Command($"asconfig freerideblocks {(!value).ToString().ToLower()}");
                OnPropertyChanged();
            }
        }

        public bool FreerideBlocksCaterpillarsTweakActive
        {
            get => _freerideBlocksCaterpillarsTweakActive;
            set
            {
                _freerideBlocksCaterpillarsTweakActive = value;
                _audiosurfHandle.Command($"asconfig freeridecaterpillars {value.ToString().ToLower()}");
                OnPropertyChanged();
            }
        }

        public bool FreerideAutoAdvanceDisableTweakActive
        {
            get => _freerideAutoAdvanceDisableTweakActive;
            set
            {
                _freerideAutoAdvanceDisableTweakActive = value;
                _audiosurfHandle.Command($"asconfig freerideautoadvance {(!value).ToString().ToLower()}");
                OnPropertyChanged();
            }
        }

        public string ConsoleContent
        {
            get => _console.ToString();
        }

        public RelayCommand EnableTweak { get; private set; }
        public RelayCommand DisableTweak { get; private set; }
        public RelayCommand SendCommand { get; private set; }
        public RelayCommand FlushConsole { get; private set; }

        private AudiosurfHandle _audiosurfHandle;
        private bool _isAudiosurfConnected;

        private bool _hiddenSongNameTweakActive;
        private bool _invisibleRoadTweakActive;
        private bool _sidewinderCameraTweakActive;
        private bool _bankingCameraTweakActive;

        private bool _freerideNoBlocksTweakActive;
        private bool _freerideBlocksCaterpillarsTweakActive;
        private bool _freerideAutoAdvanceDisableTweakActive;

        private TweakerConsole _console;
    }
}
