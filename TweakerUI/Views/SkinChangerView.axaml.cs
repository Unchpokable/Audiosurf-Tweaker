using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using TweakerUI.Models;
using TweakerUI.ViewModels;

namespace TweakerUI.Views
{
    public partial class SkinChangerView : UserControl
    {
        public SkinChangerView()
        {
            InitializeComponent();
            SkinsListBox.AddHandler(DragDrop.DropEvent, OnDrop);
        }

        // Avalonia has no XAML-only drag&drop binding (unlike the old WPF view's
        // i:EventTrigger/i:CallMethodAction pair) - AllowDrop is set in XAML, the actual handler is
        // wired here and forwards plain local paths to the view model. Avalonia 12 replaced the old
        // IDataObject-based DragEventArgs.Data with DragEventArgs.DataTransfer (IDataTransfer); file
        // paths come from the DataTransferExtensions.TryGetFiles(IDataTransfer) extension instead of
        // the old DataFormats.Files/GetFileNames() pair.
        private void OnDrop(object sender, DragEventArgs e)
        {
            var items = e.DataTransfer?.TryGetFiles();
            if (items == null)
                return;

            var files = items.Select(f => f.TryGetLocalPath()).Where(p => p != null).ToList();
            if (files.Count > 0 && DataContext is SkinChangerViewModel vm)
                vm.HandleFileDrop(files);
        }

        // Rename mode is entered from two places - the row's rename button and "export current
        // textures", which drops the new card straight into it - so the field focuses itself when it
        // appears instead of the button doing it. Two hooks, because the ListBox virtualizes: a card
        // already on screen flips IsVisible false -> true, while a card realized with RenameActive
        // already set (a fresh export added below the fold) starts visible and never gets that change.
        private void OnRenameInputPropertyChanged(object sender, AvaloniaPropertyChangedEventArgs e)
        {
            if (e.Property == Visual.IsVisibleProperty && e.NewValue is true && sender is TextBox textBox)
                FocusRenameInput(textBox);
        }

        private void OnRenameInputLoaded(object sender, RoutedEventArgs e)
        {
            if (sender is TextBox { IsVisible: true } textBox)
                FocusRenameInput(textBox);
        }

        private static void FocusRenameInput(TextBox textBox)
        {
            Dispatcher.UIThread.Post(() =>
            {
                textBox.Focus();
                textBox.SelectAll();
            }, DispatcherPriority.Loaded);
        }

        private void OnScreenshotDoubleTapped(object sender, TappedEventArgs e)
        {
            if (sender is Control control && control.DataContext is InteractableScreenshot screenshot)
                screenshot.ShowBigPictureCommand.Execute(null);
        }

        // Not a ListBox.KeyBinding: Avalonia runs KeyBindings of the focused element's ancestors BEFORE
        // the focused element sees the key, so a Delete binding here fired while typing in a card's
        // rename TextBox and removed the skin instead of a character. A bubbling KeyDown handler comes
        // after the TextBox, which marks Delete handled (Docs/Internal/avalonia-pitfalls.md, §3).
        private void OnSkinsListKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key != Key.Delete || e.KeyModifiers != KeyModifiers.None || DataContext is not SkinChangerViewModel vm)
                return;

            vm.RemoveSelectedCommand.Execute(null);
            e.Handled = true;
        }

        private void OnRenameKeyDown(object sender, KeyEventArgs e)
        {
            if (sender is not TextBox textBox || textBox.DataContext is not SkinCard card)
                return;

            if (e.Key == Key.Enter)
            {
                card.ApplyRenameCommand.Execute(null);
                e.Handled = true;
            }
            else if (e.Key == Key.Escape)
            {
                card.CancelRenameCommand.Execute(null);
                e.Handled = true;
            }
        }
    }
}
