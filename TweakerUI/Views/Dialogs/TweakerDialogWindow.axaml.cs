using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Interactivity;
using TweakerUI.Core.Dialogs;

namespace TweakerUI.Views.Dialogs
{
    public partial class TweakerDialogWindow : Window
    {
        public TweakerDialogWindow()
        {
            InitializeComponent();
        }

        private TweakerDialogWindow(string message, string caption, TweakerDialogButtons buttons) : this()
        {
            Message = message;
            Title = caption;
            ShowOk = buttons is TweakerDialogButtons.OK or TweakerDialogButtons.OKCancel;
            ShowCancel = buttons == TweakerDialogButtons.OKCancel;
            DataContext = this;
        }

        public string Message { get; }
        public bool ShowOk { get; }
        public bool ShowCancel { get; }

        public static Task<TweakerDialogResult> ShowAsync(Window owner, string message, string caption, TweakerDialogButtons buttons)
        {
            var dialog = new TweakerDialogWindow(message, caption, buttons);
            if (owner == null)
                return Task.FromResult(TweakerDialogResult.Cancel);

            // Mimics the old WPF dialog's fake-modal trick: size/position to exactly cover the owner
            // window rather than opening as a small floating box, so the dimmed top/bottom bars read
            // as "the whole app went modal" instead of a random popup.
            dialog.Width = owner.Width;
            dialog.Height = owner.Height;
            dialog.Position = owner.Position;

            return dialog.ShowDialog<TweakerDialogResult>(owner);
        }

        private void OnOkClick(object sender, RoutedEventArgs e) => Close(TweakerDialogResult.OK);

        private void OnCancelClick(object sender, RoutedEventArgs e) => Close(TweakerDialogResult.Cancel);
    }
}
