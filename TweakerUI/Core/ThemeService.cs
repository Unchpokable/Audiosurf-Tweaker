using Avalonia;
using Avalonia.Styling;

namespace TweakerUI.Core
{
    // Drives Application.RequestedThemeVariant - the single switch point both startup (App.axaml.cs,
    // reading the persisted SettingsProvider.IsDarkTheme once config is loaded) and the live Settings
    // toggle (SettingViewModel.IsDarkTheme) call into, so the two paths can't drift apart. Setting
    // RequestedThemeVariant at runtime repaints every DynamicResource-bound color immediately - no
    // restart needed, per the roadmap's Фаза 4.7 decision.
    internal static class ThemeService
    {
        internal static void Apply(bool isDark)
        {
            if (Application.Current != null)
                Application.Current.RequestedThemeVariant = isDark ? ThemeVariant.Dark : ThemeVariant.Light;
        }
    }
}
