using Avalonia.Controls;
using Avalonia.Input;
using TweakerUI.Models;
using TweakerUI.ViewModels;

namespace TweakerUI.Views
{
    public partial class ScriptDevView : UserControl
    {
        public ScriptDevView()
        {
            InitializeComponent();
        }

        // Double-click on a diagnostic opens its place in the editor. An event rather than a binding: Avalonia has
        // no double-tap command on a plain element, and this is only a hand-off to the view model's command.
        private void OnDiagnosticDoubleTapped(object sender, TappedEventArgs e)
        {
            if (sender is Control { DataContext: DiagnosticRowViewModel diagnostic } && DataContext is ScriptDevViewModel vm)
                vm.OpenDiagnosticCommand.Execute(diagnostic);
        }
    }
}
