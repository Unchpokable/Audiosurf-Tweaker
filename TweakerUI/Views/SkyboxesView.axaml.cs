using Avalonia.Controls;
using Avalonia.Input;
using TweakerCore.Skyboxes;
using TweakerUI.ViewModels;

namespace TweakerUI.Views
{
    public partial class SkyboxesView : UserControl
    {
        public SkyboxesView() => InitializeComponent();

        private void OnProblemDoubleTapped(object sender, TappedEventArgs e)
        {
            if (sender is Control { DataContext: SkyProblem problem } && DataContext is SkyboxesViewModel vm)
                vm.OpenProblemCommand.Execute(problem);
        }
    }
}
