using Avalonia.Controls;
using Avalonia.Input;
using TweakerUI.Models;
using TweakerUI.ViewModels;

namespace TweakerUI.Views
{
    public partial class ColorsConfiguratorView : UserControl
    {
        public ColorsConfiguratorView()
        {
            InitializeComponent();
        }

        // The 96x52 swatch chip is a plain Border (its whole area is the visible color, so a Button
        // retemplate would fight the visual), hence a direct pointer handler instead of a Command here.
        // Also grabs keyboard focus - otherwise clicking it while the name TextBox is focused leaves
        // the TextBox's caret/selection visibly stuck (Border isn't focusable by default, so nothing
        // would normally take focus away from it).
        private void OnSwatchPointerPressed(object sender, PointerPressedEventArgs e)
        {
            if (sender is Border { DataContext: SwatchEditState swatch } border)
            {
                border.Focus();
                if (DataContext is ColorsConfiguratorViewModel vm)
                    vm.SelectSwatchCommand.Execute(swatch);
            }
        }
    }
}
