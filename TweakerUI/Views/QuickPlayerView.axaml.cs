using System.Linq;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using Avalonia.VisualTree;
using TweakerUI.Models;
using TweakerUI.ViewModels;

namespace TweakerUI.Views
{
    public partial class QuickPlayerView : UserControl
    {
        public QuickPlayerView()
        {
            InitializeComponent();
            QueueListBox.AddHandler(DragDrop.DropEvent, OnDrop);
        }

        // Same pattern as SkinChangerView's SkinsListBox drop handler - Avalonia has no XAML-only
        // drag&drop binding, AllowDrop is set in XAML, the handler forwards local paths to the VM.
        private void OnDrop(object sender, DragEventArgs e)
        {
            var items = e.DataTransfer?.TryGetFiles();
            if (items == null)
                return;

            var files = items.Select(f => f.TryGetLocalPath()).Where(p => p != null).ToList();
            if (files.Count > 0 && DataContext is QuickPlayerViewModel vm)
                _ = vm.AddFilesAsync(files);
        }

        // Same reasoning as SkinChangerView.OnRenameClick - entering rename mode also needs to move
        // keyboard focus into the now-visible TextBox, which only a code-behind click handler can do.
        private void OnPlaylistRenameClick(object sender, Avalonia.Interactivity.RoutedEventArgs e)
        {
            if (sender is not Button button || button.DataContext is not PlaylistRowViewModel row)
                return;

            row.EnableRenameCommand.Execute(null);

            Dispatcher.UIThread.Post(() =>
            {
                var textBox = button.FindAncestorOfType<Grid>()?
                    .GetVisualDescendants()
                    .OfType<TextBox>()
                    .FirstOrDefault(t => t.Classes.Contains("QPRenameInput"));

                textBox?.Focus();
                textBox?.SelectAll();
            }, DispatcherPriority.Loaded);
        }

        private void OnPlaylistRenameKeyDown(object sender, KeyEventArgs e)
        {
            if (sender is not TextBox textBox || textBox.DataContext is not PlaylistRowViewModel row)
                return;

            if (e.Key == Key.Enter)
            {
                row.ApplyRenameCommand.Execute(null);
                e.Handled = true;
            }
            else if (e.Key == Key.Escape)
            {
                row.CancelRenameCommand.Execute(null);
                e.Handled = true;
            }
        }
    }
}
