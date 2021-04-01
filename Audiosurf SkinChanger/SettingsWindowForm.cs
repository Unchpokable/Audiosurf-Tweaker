namespace Audiosurf_SkinChanger
{
    using System.Windows.Forms;
    using System;
    using System.Configuration;
    using Audiosurf_SkinChanger.Utilities;
    using System.IO;

    public partial class SettingsWindowForm : Form
    {
        public event Action OnSettingsApplied;
        public SettingsWindowForm()
        {
            InitializeComponent();
            behaviourSelector.SelectedIndex = int.Parse(ConfigurationManager.AppSettings.Get("DCSBehaviour"));
            allowWarnCheck.Checked = EnvironmentalVeriables.DCSWarningsAllowed;
            gamePathEntry.Text = EnvironmentalVeriables.gamePath;
            addFolderPath.Text = EnvironmentalVeriables.skinsFolderPath;
        }

        private void ApplySettings(object sender, EventArgs e) //OK button
        {
            Configuration cfg = ConfigurationManager.OpenExeConfiguration(ConfigurationUserLevel.None);
            EnvironmentalVeriables.gamePath = gamePathEntry.Text;
            EnvironmentalVeriables.skinsFolderPath = addFolderPath.Text;
            EnvironmentalVeriables.ControlSystemBehaviour = TranslateSelectedBehaviourString();
            EnvironmentalVeriables.DCSWarningsAllowed = allowWarnCheck.Checked;
            cfg.AppSettings.Settings["AllowWarnings"].Value = allowWarnCheck.Checked.ToString();
            cfg.AppSettings.Settings["DCSBehaviour"].Value = ConvertDCSBehaveIntoNumericValue().ToString();
            cfg.Save();
            ConfigurationManager.RefreshSection("appSettings");
            OnSettingsApplied?.Invoke();
        }

        private DCSBehaviour TranslateSelectedBehaviourString()
        {
            switch (((string)behaviourSelector.SelectedItem).ToLower())
            {
                case "on boot":
                    return DCSBehaviour.OnBoot;
                case "async after boot":
                    return DCSBehaviour.AsyncAfterBoot;
                default:
                    throw new Exception("Cant translate selected Combobox value into DCSBehaviour enum value");
            }
        }

        private int ConvertDCSBehaveIntoNumericValue()
        {
            switch (EnvironmentalVeriables.ControlSystemBehaviour)
            {
                case DCSBehaviour.OnBoot:
                    return 0;
                case DCSBehaviour.AsyncAfterBoot:
                    return 1;
                default:
                    return int.MinValue;
            }
        }

        private void OpenFileSelectionDialog(object sender, EventArgs e)
        {
            if (folderBrowserDialog1.ShowDialog() == DialogResult.OK)
            {
                var knownSender = sender as Button;

                if (Directory.Exists(folderBrowserDialog1.SelectedPath))
                {
                    if (knownSender.Name == "gamePathViewBtn")
                        gamePathEntry.Text = folderBrowserDialog1.SelectedPath;

                    else if (knownSender.Name == "addSkinsPathBtn")
                        addFolderPath.Text = folderBrowserDialog1.SelectedPath;
                }
            }
        }

        private void ApplyAndClose(object sender, EventArgs e)
        {
            ApplySettings(sender, e);
            Close();
        }

        private void Close(object sender, EventArgs e)
        {
            Close();
        }
    }
}
