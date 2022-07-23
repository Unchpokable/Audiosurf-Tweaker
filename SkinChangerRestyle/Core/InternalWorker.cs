namespace SkinChangerRestyle.Core
{
    using System;
    using System.Configuration;
    using Microsoft.Win32;
    using System.IO;
    using Env = SkinChangerRestyle.Core.EnvironmentContainer;

    internal delegate void ExternExceptionHandler(Exception innerException);

    internal static class InternalWorker
    {
        internal static ExternExceptionHandler InitializationFaultCallback { private get; set; }
        internal static string FingerPrint;

        private static string localMachineSubkeyName = "ASCHDATA";

        private static void RegisterApplication()
        {
            var globalTimeTickOffset = DateTime.Now.Ticks;
            var tempGUID = Guid.NewGuid();
            FingerPrint = $"{globalTimeTickOffset}-{tempGUID}";
            using (RegistryKey regKey = Registry.LocalMachine.CreateSubKey(localMachineSubkeyName))
            {
                regKey.SetValue("ID", FingerPrint);
            }
        }

        public static void SetUpDefaultSettings()
        {
            if (Registry.LocalMachine.OpenSubKey(localMachineSubkeyName) != null)
                return;

            Configuration cfg = ConfigurationManager.OpenExeConfiguration(ConfigurationUserLevel.None);

            var pathToAudiosurfTextures = System.Environment.Is64BitOperatingSystem
                                          ? Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Valve\Steam", "InstallPath", null).ToString()
                                          : Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Valve\Steam", "InstallPath", null).ToString();

            if (string.IsNullOrEmpty(pathToAudiosurfTextures) 
                || string.IsNullOrWhiteSpace(pathToAudiosurfTextures)
                || !Directory.Exists(pathToAudiosurfTextures + @"\steamapps\common\Audiosurf"))
            {
                InitializationFaultCallback?.Invoke(new Exception("Couldn't find Audiosurf pathes or registry access denied by operating system"));
                return;
            }

            //RegisterApplication();
            cfg.AppSettings.Settings["gamePath"].Value = pathToAudiosurfTextures + @"\steamapps\common\Audiosurf\engine\textures";
            cfg.Save();
            ConfigurationManager.RefreshSection("appSettings");
        }

        public static void InitializeEnvironment()
        {
            try
            {
                Env.gamePath = ConfigurationManager.AppSettings.Get("gamePath");
                Env.skinsFolderPath = ConfigurationManager.AppSettings.Get("skinsPath");
                Env.ControlSystemBehaviour = ParseBehaviourFromConfig();
                Env.DCSWarningsAllowed = ParseIsWarningsAllowedFromConfig();
            }
            catch (Exception e)
            {
                InitializationFaultCallback.Invoke(e);
                return;
            }
        }

        private static DCSBehaviour ParseBehaviourFromConfig()
        {
            var currentValue = ConfigurationManager.AppSettings.Get("DCSBehaviour");
            switch (currentValue)
            {
                case "0":
                    return DCSBehaviour.OnBoot;
                case "1":
                    return DCSBehaviour.AsyncAfterBoot;
                default:
                    throw new Exception("Wrong Configuration parameter");
            }
        }

        private static bool ParseIsWarningsAllowedFromConfig()
        {
            return bool.Parse(ConfigurationManager.AppSettings.Get("AllowWarnings"));
        }
    }
}
