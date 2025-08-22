using CommunityToolkit.Mvvm.ComponentModel;

namespace Tweaker.ViewModels;

public partial class MainWindowViewModel : ObservableObject
{
    [ObservableProperty]
    private string? applicationIcon;

    public MainWindowViewModel()
    {
        ApplicationIcon = "/icon.ico";
    }
}