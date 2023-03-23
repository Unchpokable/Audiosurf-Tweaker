using SkinChangerRestyle.Core;
using ASCommander;
using SkinChangerRestyle.MVVM.Model;
using ChangerAPI.Utilities;
using System;
using System.Collections.Generic;

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
                if (string.Equals(param.ToString(), "closeaudiosurf", StringComparison.OrdinalIgnoreCase))
                {
                    KillAudiosurf();
                    return;
                }
                _audiosurfHandle.Command($"ascommand {param}");
            });

            FlushConsole = new RelayCommand(param =>
            {
                _console.Flush();
                OnPropertyChanged(nameof(ConsoleContent));
            });

            _externalTweaksFields = new Dictionary<string, Reference<bool>>()
            {
                {"InvisibleRoad", _invisibleRoadTweakActive },
                {"HiddenSong", _hiddenSongNameTweakActive },
                {"SidewinderCamera", _sidewinderCameraTweakActive },
                {"BankingCamera", _bankingCameraTweakActive }
            };

            _audiosurfHandle.MessageResieved += OnMessageRecieved;
        }

        ~TweakerViewModel()
        {
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
                _invisibleRoadTweakActive.Value = value;
                _audiosurfHandle.Command($"asconfig roadvisible {(!value).ToString().ToLower()}");
                OnPropertyChanged();
            }
        }

        public bool HiddenSongTweakActive
        {
            get => _hiddenSongNameTweakActive; 
            set 
            { 
                _hiddenSongNameTweakActive.Value = value;
                _audiosurfHandle.Command($"asconfig showsongname {(!value).ToString().ToLower()}");
                OnPropertyChanged();
            }
        }

        public bool SidewinderCameraTweakActive
        {
            get => _sidewinderCameraTweakActive;
            set
            {
                _sidewinderCameraTweakActive.Value = value;
                _audiosurfHandle.Command($"asconfig sidewinder {value.ToString().ToLower()}");
                OnPropertyChanged();
            }
        }

        public bool BankingCameraTweakActive
        {
            get => _bankingCameraTweakActive;
            set
            {
                _bankingCameraTweakActive.Value = value;
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

        private Reference<bool> _hiddenSongNameTweakActive = new Reference<bool>(false);
        private Reference<bool> _invisibleRoadTweakActive = new Reference<bool>(false);
        private Reference<bool> _sidewinderCameraTweakActive = new Reference<bool>(false);
        private Reference<bool> _bankingCameraTweakActive = new Reference<bool>(false);

        private bool _freerideNoBlocksTweakActive;
        private bool _freerideBlocksCaterpillarsTweakActive;
        private bool _freerideAutoAdvanceDisableTweakActive;

        private TweakerConsole _console;

        private Dictionary<string, Reference<bool>> _externalTweaksFields; //TODO: rename it

        private void OnMessageRecieved(object sender, string message)
        {
            if (message.Contains("tw-Notify-Tweak-Changed"))
            {
                var propChanged = message.Substring("tw-Notify-Tweak-Changed".Length + 1).Split(' ');
                if (propChanged.Length != 2)
                    return;
                var fieldName = propChanged[0];
                var targetField = _externalTweaksFields[fieldName];
                targetField.Value = bool.Parse(propChanged[1]);
                OnPropertyChanged(fieldName + "TweakActive");
            }
        }

        private void KillAudiosurf()
        {
            Core.Extensions.Extensions.Cmd($"taskkill /f /im \"{AppDomain.CurrentDomain.FriendlyName}\"");
        }

        private class Reference<T>
            where T : struct
        {
            public Reference(T value)
            {
                Value = value;
            }

            public T Value { get; set; }

            public static implicit operator T(Reference<T> reference) => reference.Value;
        }
    }
}
