using System.Linq;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using Avalonia.VisualTree;
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

        // Not bound as a Command like the other row buttons: entering rename mode also needs to move
        // keyboard focus into the now-visible TextBox, which only a code-behind click handler can do
        // (same reasoning as HsvWheel/GradientSlider's explicit Focus() calls in 4.2).
        private void OnRenameClick(object sender, RoutedEventArgs e)
        {
            if (sender is not Button button || button.DataContext is not SkinCard card)
                return;

            card.EnableRenameCommand.Execute(null);

            Dispatcher.UIThread.Post(() =>
            {
                var textBox = button.FindAncestorOfType<Grid>()?
                    .GetVisualDescendants()
                    .OfType<TextBox>()
                    .FirstOrDefault(t => t.Classes.Contains("SCRenameInput"));

                textBox?.Focus();
                textBox?.SelectAll();
            }, DispatcherPriority.Loaded);
        }

        private void OnScreenshotDoubleTapped(object sender, TappedEventArgs e)
        {
            if (sender is Control control && control.DataContext is InteractableScreenshot screenshot)
                screenshot.ShowBigPictureCommand.Execute(null);
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
