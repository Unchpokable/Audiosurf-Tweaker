using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Threading;
using TweakerUI.Core.Dialogs;

namespace TweakerUI.Views.Dialogs
{
    public partial class TweakerDialogWindow : Window
    {
        public TweakerDialogWindow()
        {
            InitializeComponent();
        }

        private TweakerDialogWindow(string message, string caption, IReadOnlyList<TweakerDialogChoice> choices) : this()
        {
            Message = message;
            Title = caption;
            DataContext = this;

            // Pre-set rather than left at zero: a dialog dismissed instead of answered (Alt+F4 - there is no
            // chrome to click) must not read as pressing the first button, which is the acting one.
            Result = IndexOfCancel(choices);

            for (var i = 0; i < choices.Count; i++)
            {
                var index = i;
                var button = new Button
                {
                    Content = choices[i].Text,
                    MinWidth = 80,
                    HorizontalContentAlignment = HorizontalAlignment.Center
                };

                button.Click += (_, _) =>
                {
                    Result = index;
                    Close();
                };

                ButtonsPanel.Children.Add(button);
            }
        }

        public string Message { get; }

        /// <summary>Index of the pressed choice; the cancelling one if the dialog was dismissed.</summary>
        private int Result { get; set; }

        /// <summary>
        /// The two-outcome dialog every existing call site uses. Built on the same N-button machinery below -
        /// OK/Cancel is just the list [OK, Cancel].
        /// </summary>
        public static async Task<TweakerDialogResult> ShowAsync(Window owner, string message, string caption, TweakerDialogButtons buttons)
        {
            if (buttons == TweakerDialogButtons.OK)
            {
                await ShowChoiceAsync(owner, message, caption, new[] { new TweakerDialogChoice("OK", isCancel: true) });
                return TweakerDialogResult.OK;
            }

            var index = await ShowChoiceAsync(owner, message, caption, TweakerDialogChoice.Of("OK", "Cancel"));
            return index == 0 ? TweakerDialogResult.OK : TweakerDialogResult.Cancel;
        }

        /// <summary>
        /// Shows one button per choice and returns the index of the one pressed. With no owner window - or when
        /// the dialog is dismissed rather than answered - the cancelling choice is what comes back, so a caller
        /// that cannot ask never ends up taking the acting branch by default.
        /// </summary>
        /// <param name="closeWhen">
        /// Dismisses the dialog from code - for a dialog that is really a wait with a way out, where whatever is
        /// being waited for can arrive first (PluginService's "waiting for Audiosurf to close"). The result is
        /// the cancelling choice either way; which of the two happened is the caller's to tell apart, from the
        /// thing it was waiting on rather than from the dialog.
        /// </param>
        public static async Task<int> ShowChoiceAsync(
            Window owner,
            string message,
            string caption,
            IReadOnlyList<TweakerDialogChoice> choices,
            CancellationToken closeWhen = default)
        {
            if (owner == null || choices == null || choices.Count == 0)
                return IndexOfCancel(choices);

            var dialog = new TweakerDialogWindow(message, caption, choices);

            using var registration = closeWhen.CanBeCanceled
                ? closeWhen.Register(() => Dispatcher.UIThread.Post(() => dialog.Close()))
                : default;

            // Mimics the old WPF dialog's fake-modal trick: size/position to exactly cover the owner
            // window rather than opening as a small floating box, so the dimmed top/bottom bars read
            // as "the whole app went modal" instead of a random popup.
            dialog.Width = owner.Width;
            dialog.Height = owner.Height;
            dialog.Position = owner.Position;

            await dialog.ShowDialog(owner);
            return dialog.Result;
        }

        private static int IndexOfCancel(IReadOnlyList<TweakerDialogChoice> choices)
        {
            if (choices == null || choices.Count == 0)
                return -1;

            for (var i = 0; i < choices.Count; i++)
            {
                if (choices[i].IsCancel)
                    return i;
            }

            return choices.Count - 1;
        }
    }
}
