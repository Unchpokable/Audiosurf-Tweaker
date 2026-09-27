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

        // Wheel (160 + CCWheelWrap's 2x14 padding) + its 20px gap + the narrowest the sliders are still
        // comfortable to drag at. Below this the controls go under the wheel.
        private const double EditorWideLayoutMinWidth = 188 + 20 + 280;

        private void OnEditorLayoutSizeChanged(object sender, SizeChangedEventArgs e)
        {
            var wide = e.NewSize.Width >= EditorWideLayoutMinWidth;
            Grid.SetColumn(EditorControls, wide ? 1 : 0);
            Grid.SetRow(EditorControls, wide ? 0 : 1);
            // Stacked, the controls span both columns: column 0 alone is only as wide as the wheel.
            Grid.SetColumnSpan(EditorControls, wide ? 1 : 2);
        }

        // The swatch chip is a plain Border (its whole area is the visible color, so a Button
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
