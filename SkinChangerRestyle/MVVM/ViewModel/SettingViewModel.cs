using SkinChangerRestyle.Core;
using SkinChangerRestyle.Properties;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace SkinChangerRestyle.MVVM.ViewModel
{
    public enum SettingsFields
    {
        TexturesPath,
        AddSkinsPath
    }

    internal class SettingViewModel : ObservableObject
    {
        public string TexturesFolderPath
        {
            get => _texturesPath;
            set
            {
                _texturesPath = value;
                SettingsProvider.GameTexturesPath = value;
                ApplySettings();
                OnPropertyChanged();
            }
        }
        public string AdditionalSkinsFolderPath
        {
            get => _additionalSkinsPath;
            set
            {
                _additionalSkinsPath = value;
                SettingsProvider.SkinsFolderPath = value;
                ApplySettings();
                OnPropertyChanged();
            }
        }

        public bool IsShouldCheckTextures
        {
            get => _isShouldCheckTextures;
            set
            {
                _isShouldCheckTextures = value;
                SettingsProvider.ControlSystemActive = value;
                ApplySettings();
                OnPropertyChanged();
            }
        }

        public bool IsHotReload
        {
            get => _isHotReload;
            set
            {
                _isHotReload = value;
                SettingsProvider.HotReload = value;
                ApplySettings();
                OnPropertyChanged();
            }
        }
        
        public bool IsShouldUseSafetyInstallation
        {
            get => _isSafeInstall;
            set
            {
                _isSafeInstall = value;
                SettingsProvider.SafeInstall = value;
                ApplySettings();
                OnPropertyChanged();
            }
        }

        public RelayCommand SetConfigurationValue { get; set; }

        private string _texturesPath;
        private string _additionalSkinsPath;
        private bool _isShouldCheckTextures;
        private bool _isHotReload;
        private bool _isSafeInstall;

        public SettingViewModel()
        {
            try
            {
                SetConfigurationValue = new RelayCommand(AskAndSetConfigValue);
                TexturesFolderPath = SettingsProvider.GameTexturesPath;
                AdditionalSkinsFolderPath = SettingsProvider.SkinsFolderPath;
                IsHotReload = SettingsProvider.HotReload;
                IsShouldCheckTextures = SettingsProvider.ControlSystemActive;
            }
            catch(Exception e)
            {
                File.WriteAllText("SVM.txt", e.Message);
            }
        }

        private void AskAndSetConfigValue(object parameter)
        {
            var field = (SettingsFields)parameter;

            var pathSelectionDialog = new FolderBrowserDialog();
            if (pathSelectionDialog.ShowDialog() == DialogResult.OK)
            {
                switch (field)
                {
                    case SettingsFields.TexturesPath:
                        TexturesFolderPath = pathSelectionDialog.SelectedPath;
                        SettingsProvider.GameTexturesPath = TexturesFolderPath;
                        break;
                    case SettingsFields.AddSkinsPath:
                        AdditionalSkinsFolderPath = pathSelectionDialog.SelectedPath;
                        SettingsProvider.SkinsFolderPath = AdditionalSkinsFolderPath;
                        break;
                }
                ApplySettings();
            }
        }

        private void ApplySettings()
        {
            InternalWorker.RewriteSettings();
        }
    }
}
