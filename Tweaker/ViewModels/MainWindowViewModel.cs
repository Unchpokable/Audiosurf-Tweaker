using CommunityToolkit.Mvvm.ComponentModel;
using Tweaker.Settings;
using Tweaker.Core.Errors;

namespace Tweaker.ViewModels;

public partial class MainWindowViewModel : ObservableObject
{
    private readonly IConfigurationService _configurationService;

    [ObservableProperty]
    private string? applicationIcon;

    [ObservableProperty]
    private string statusMessage = string.Empty;

    [ObservableProperty]
    private string defaultGamePath = string.Empty;

    public MainWindowViewModel()
    {
        _configurationService = new ConfigurationService();
        ApplicationIcon = "/icon.ico";

        LoadConfiguration();
    }

    private void LoadConfiguration()
    {
        if (_configurationService.State == ConfigurationState.Invalid)
        {
            StatusMessage = "Configuration failed to load";
            return;
        }

        var appSettingsResult = _configurationService.GetSection<AppSettings>("AppSettings");
        appSettingsResult.Match(
            onSuccess: settings =>
            {
                DefaultGamePath = settings.DefaultGamePath;
                StatusMessage = "Configuration loaded successfully";
            },
            onError: error =>
            {
                StatusMessage = $"Configuration error: {error.Message}";
            }
        );
    }
}