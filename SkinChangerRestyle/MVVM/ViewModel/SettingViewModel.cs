using SkinChangerRestyle.Core;
using System;
using System.Collections.Generic;
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
                OnPropertyChanged();
            }
        }
        public string AdditionalSkinsFolderPath
        {
            get => _additionalSkinsPath;
            set
            {
                _additionalSkinsPath = value;
                OnPropertyChanged();
            }
        }

        public bool IsShouldCheckTextures
        {
            get => _isShouldCheckTextures;
            set
            {
                _isShouldCheckTextures = value;
                OnPropertyChanged();
            }
        }

        public bool IsHotReload { get; set; }
        

        public RelayCommand SetConfigurationValue { get; set; }

        private string _texturesPath;
        private string _additionalSkinsPath;
        private bool _isShouldCheckTextures;

        public SettingViewModel()
        {
            SetConfigurationValue = new RelayCommand(AskAndSetConfigValue);
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
                        return;
                    case SettingsFields.AddSkinsPath:
                        AdditionalSkinsFolderPath = pathSelectionDialog.SelectedPath;
                        return;
                }
            }
        }
    }
}
