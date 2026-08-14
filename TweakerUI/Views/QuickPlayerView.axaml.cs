using System;
using System.Linq;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using Avalonia.VisualTree;
using TweakerUI.Core;
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
            QueueListBox.AddHandler(DragDrop.DragOverEvent, OnQueueDragOver);
            QueueListBox.AddHandler(DragDrop.DragLeaveEvent, OnQueueDragLeave);

            // Tunnelling: ListBoxItem handles the press for selection on the way back up, and this has
            // to see it first to remember where the drag would have started from. Nothing here marks
            // the event handled, so clicking a card still selects it as before.
            QueueListBox.AddHandler(PointerPressedEvent, OnQueuePointerPressed, RoutingStrategies.Tunnel);
            QueueListBox.AddHandler(PointerMovedEvent, OnQueuePointerMoved, RoutingStrategies.Tunnel);
            QueueListBox.AddHandler(PointerReleasedEvent, OnQueuePointerReleased, RoutingStrategies.Tunnel);
            QueueListBox.AddHandler(PointerCaptureLostEvent, OnQueuePointerCaptureLost, RoutingStrategies.Tunnel);
        }

        // In-process format: the payload never leaves the app, so the card view model travels by
        // reference and nothing has to be serialised or looked up again on the other side. It is also
        // what tells an internal reorder apart from files dragged in from Explorer, which land on this
        // same list (see OnDrop).
        private static readonly DataFormat<TrackCardViewModel> QueueCardFormat =
            DataFormat.CreateInProcessFormat<TrackCardViewModel>("audiosurf-tweaker/queue-card");

        /// <summary>How far the pointer has to travel before a press on a card becomes a drag rather than a click.</summary>
        private const double DragThreshold = 4;

        private const double AutoScrollMargin = 36;
        private const double AutoScrollStep = 12;

        // DoDragDropAsync takes the original PointerPressedEventArgs, but the drag must not begin on the
        // press itself - that would swallow every plain click on a card into a drag loop. So the press
        // is held until the pointer has actually moved.
        private PointerPressedEventArgs _queuePress;
        private TrackCardViewModel _pressedCard;
        private Point _pressOrigin;
        private bool _dragging;

        // Only one card shows an insertion line at a time; remembering which avoids walking the whole
        // queue on every DragOver, and those arrive continuously while the pointer moves.
        private TrackCardViewModel _indicatorCard;

        private DispatcherTimer _autoScrollTimer;
        private double _autoScrollDelta;

        private void OnQueuePointerPressed(object sender, PointerPressedEventArgs e)
        {
            ResetQueuePress();

            var point = e.GetCurrentPoint(QueueListBox);
            if (!point.Properties.IsLeftButtonPressed)
                return;

            var card = CardUnder(e.Source);
            if (card == null)
                return;

            // The card's own buttons (remove) keep their click - starting a drag from one would make it
            // awkward to hit and would never be what the user meant.
            if ((e.Source as Visual)?.FindAncestorOfType<Button>(includeSelf: true) != null)
                return;

            _queuePress = e;
            _pressedCard = card;
            _pressOrigin = point.Position;
        }

        private void OnQueuePointerMoved(object sender, PointerEventArgs e)
        {
            if (_queuePress == null || _dragging)
                return;

            var position = e.GetPosition(QueueListBox);
            if (Math.Abs(position.X - _pressOrigin.X) < DragThreshold && Math.Abs(position.Y - _pressOrigin.Y) < DragThreshold)
                return;

            _ = BeginQueueDragAsync(_queuePress, _pressedCard);
        }

        private void OnQueuePointerReleased(object sender, PointerReleasedEventArgs e)
        {
            if (!_dragging)
                ResetQueuePress();
        }

        private void OnQueuePointerCaptureLost(object sender, PointerCaptureLostEventArgs e)
        {
            if (!_dragging)
                ResetQueuePress();
        }

        private async Task BeginQueueDragAsync(PointerPressedEventArgs press, TrackCardViewModel card)
        {
            _dragging = true;
            card.IsDragged = true;

            try
            {
                var transfer = new DataTransfer();
                transfer.Add(DataTransferItem.Create(QueueCardFormat, card));

                // Not disposed here on purpose - DoDragDropAsync takes ownership of the transfer and
                // disposes it itself once the operation finishes.
                await DragDrop.DoDragDropAsync(press, transfer, DragDropEffects.Move);
            }
            catch (Exception ex)
            {
                // Nothing awaits this method, so an exception here would otherwise vanish into an
                // unobserved task and read as "dragging just doesn't do anything".
                ApplicationNotificationManager.Manager.ShowError("Quick Player", $"Could not reorder the queue: {ex.Message}");
            }
            finally
            {
                card.IsDragged = false;
                ClearDropIndicator();
                StopAutoScroll();
                ResetQueuePress();
            }
        }

        private void ResetQueuePress()
        {
            _queuePress = null;
            _pressedCard = null;
            _dragging = false;
        }

        private void OnQueueDragOver(object sender, DragEventArgs e)
        {
            if (e.DataTransfer?.Contains(QueueCardFormat) == true)
            {
                e.DragEffects = DragDropEffects.Move;
                ShowDropIndicator(e);
                UpdateAutoScroll(e);
            }
            else if (e.DataTransfer?.Contains(DataFormat.File) == true)
            {
                // Files from outside are appended, not inserted at the pointer - hence no indicator.
                e.DragEffects = DragDropEffects.Copy;
                ClearDropIndicator();
            }
            else
            {
                e.DragEffects = DragDropEffects.None;
                ClearDropIndicator();
            }

            e.Handled = true;
        }

        private void OnQueueDragLeave(object sender, DragEventArgs e)
        {
            ClearDropIndicator();
            StopAutoScroll();
        }

        // Two drops arrive on this list and mean entirely different things: a card from the list itself
        // (reorder) and files from Explorer (add). Same pattern as SkinChangerView's SkinsListBox handler
        // for the second half - Avalonia has no XAML-only drag&drop binding, AllowDrop is set in XAML,
        // and the handler forwards to the VM.
        private void OnDrop(object sender, DragEventArgs e)
        {
            ClearDropIndicator();
            StopAutoScroll();

            if (DataContext is not QuickPlayerViewModel vm)
                return;

            var card = e.DataTransfer?.TryGetValue(QueueCardFormat);
            if (card != null)
            {
                var from = vm.Queue.IndexOf(card);
                if (from >= 0)
                    vm.MoveTrack(from, ResolveDropIndex(e, vm, from));

                e.Handled = true;
                return;
            }

            var items = e.DataTransfer?.TryGetFiles();
            if (items == null)
                return;

            var files = items.Select(f => f.TryGetLocalPath()).Where(p => p != null).ToList();
            if (files.Count > 0)
                _ = vm.AddFilesAsync(files);
        }

        /// <summary>
        /// Where the dragged card ends up, in ObservableCollection.Move's terms - an index into the list
        /// as it is *with the card already lifted out of it*. The pointer answers a different question
        /// ("before or after the card it is over"), and every insertion point past the card's own
        /// position is one lower once the card is gone; getting that conversion wrong is an off-by-one
        /// that only shows up when dragging downwards.
        /// </summary>
        private int ResolveDropIndex(DragEventArgs e, QuickPlayerViewModel vm, int from)
        {
            var item = ItemAt(e);
            var insertion = vm.Queue.Count;

            if (item?.DataContext is TrackCardViewModel target)
            {
                var index = vm.Queue.IndexOf(target);
                if (index >= 0)
                    insertion = IsBelowMidpoint(e, item) ? index + 1 : index;
            }

            return insertion > from ? insertion - 1 : insertion;
        }

        private void ShowDropIndicator(DragEventArgs e)
        {
            var item = ItemAt(e);
            if (item?.DataContext is not TrackCardViewModel target)
            {
                ClearDropIndicator();
                return;
            }

            if (_indicatorCard != null && _indicatorCard != target)
                _indicatorCard.ClearDropIndicator();

            _indicatorCard = target;

            var after = IsBelowMidpoint(e, item);
            target.DropBefore = !after;
            target.DropAfter = after;
        }

        private void ClearDropIndicator()
        {
            _indicatorCard?.ClearDropIndicator();
            _indicatorCard = null;
        }

        // Dragging to the very top or bottom of a long playlist otherwise means dropping, scrolling and
        // dragging again. Driven by a timer rather than by DragOver, because the pointer sitting still
        // at the edge raises no further events.
        private void UpdateAutoScroll(DragEventArgs e)
        {
            var height = QueueScroll.Bounds.Height;
            var y = e.GetPosition(QueueScroll).Y;

            if (y < AutoScrollMargin)
                _autoScrollDelta = -AutoScrollStep;
            else if (y > height - AutoScrollMargin)
                _autoScrollDelta = AutoScrollStep;
            else
                _autoScrollDelta = 0;

            if (_autoScrollDelta == 0)
            {
                StopAutoScroll();
                return;
            }

            if (_autoScrollTimer != null)
                return;

            _autoScrollTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(30) };
            _autoScrollTimer.Tick += OnAutoScrollTick;
            _autoScrollTimer.Start();
        }

        private void OnAutoScrollTick(object sender, EventArgs e)
        {
            var offset = QueueScroll.Offset;
            var max = Math.Max(0, QueueScroll.Extent.Height - QueueScroll.Viewport.Height);
            QueueScroll.Offset = new Vector(offset.X, Math.Clamp(offset.Y + _autoScrollDelta, 0, max));
        }

        private void StopAutoScroll()
        {
            if (_autoScrollTimer == null)
                return;

            _autoScrollTimer.Stop();
            _autoScrollTimer.Tick -= OnAutoScrollTick;
            _autoScrollTimer = null;
            _autoScrollDelta = 0;
        }

        private static bool IsBelowMidpoint(DragEventArgs e, ListBoxItem item) =>
            e.GetPosition(item).Y > item.Bounds.Height / 2;

        /// <summary>
        /// The row the pointer is over during a drag, by hit-testing rather than by reading e.Source:
        /// which element a drag event names as its source depends on how the drop target was resolved,
        /// and the answer here has to be the row specifically. Returns null past the last row.
        /// </summary>
        private ListBoxItem ItemAt(DragEventArgs e) =>
            (QueueListBox.InputHitTest(e.GetPosition(QueueListBox)) as Visual)?.FindAncestorOfType<ListBoxItem>(includeSelf: true);

        private static TrackCardViewModel CardUnder(object source) =>
            (source as Visual)?.FindAncestorOfType<ListBoxItem>(includeSelf: true)?.DataContext as TrackCardViewModel;

        // Same reasoning as SkinChangerView.OnRenameClick - entering rename mode also needs to move
        // keyboard focus into the now-visible TextBox, which only a code-behind click handler can do.
        private void OnPlaylistRenameClick(object sender, RoutedEventArgs e)
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
