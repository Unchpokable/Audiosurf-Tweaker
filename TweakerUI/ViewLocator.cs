using Avalonia.Controls;
using Avalonia.Controls.Templates;
using TweakerUI.ViewModels;
using TweakerUI.Views;

namespace TweakerUI
{
    public class ViewLocator : IDataTemplate
    {
        public Control Build(object data) => data switch
        {
            SkinChangerViewModel => new SkinChangerView(),
            ColorsConfiguratorViewModel => new ColorsConfiguratorView(),
            TweakerViewModel => new TweakerView(),
            QuickPlayerViewModel => new QuickPlayerView(),
            ScriptsViewModel => new ScriptsView(),
            ScriptDevViewModel => new ScriptDevView(),
            SkyboxesViewModel => new SkyboxesView(),
            SettingViewModel => new SettingView(),
            _ => null
        };

        public bool Match(object data) => data is SkinChangerViewModel or ColorsConfiguratorViewModel
            or TweakerViewModel or QuickPlayerViewModel or ScriptsViewModel or ScriptDevViewModel
            or SkyboxesViewModel or SettingViewModel;
    }
}
