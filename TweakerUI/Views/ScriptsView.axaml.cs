using System;
using System.Linq;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.VisualTree;
using TweakerUI.Core;
using TweakerUI.Models;
using TweakerUI.ViewModels;

namespace TweakerUI.Views
{
    /// <summary>
    /// The Scripts tab. The code here is the drag that changes the run order (Docs/Internal/script-coexistence.md
    /// §3): the same DragDrop pattern as Quick Player's queue (QuickPlayerView), started from a row's handle only - the
    /// rest of a row has its own switch and buttons. Where the row lands and what that means is the view model's
    /// (<see cref="ScriptsViewModel.MoveScript"/>).
    /// </summary>
    public partial class ScriptsView : UserControl
    {
        // In-process: the row travels by reference, and nothing from outside the app can be dropped here.
        private static readonly DataFormat<ScriptRowViewModel> RowFormat =
            DataFormat.CreateInProcessFormat<ScriptRowViewModel>("audiosurf-tweaker/script-row");

        /// <summary>How far the pointer has to travel before a press on a handle becomes a drag.</summary>
        private const double DragThreshold = 4;

        private readonly DragAutoScroll _autoScroll;

        // DoDragDropAsync takes the original press, but the drag must not begin on the press itself - so it is held
        // until the pointer has actually moved (the same as QuickPlayerView).
        private PointerPressedEventArgs _press;
        private ScriptRowViewModel _pressedRow;
        private Point _pressOrigin;
        private bool _dragging;

        private ScriptRowViewModel _indicatorRow;

        public ScriptsView()
        {
            InitializeComponent();
            _autoScroll = new DragAutoScroll(ScriptsScroll);

            ScriptsList.AddHandler(DragDrop.DropEvent, OnDrop);
            ScriptsList.AddHandler(DragDrop.DragOverEvent, OnDragOver);
            ScriptsList.AddHandler(DragDrop.DragLeaveEvent, OnDragLeave);
            ScriptsList.AddHandler(PointerPressedEvent, OnPointerPressed, RoutingStrategies.Tunnel);
            ScriptsList.AddHandler(PointerMovedEvent, OnPointerMoved, RoutingStrategies.Tunnel);
            ScriptsList.AddHandler(PointerReleasedEvent, (_, _) => EndPress(), RoutingStrategies.Tunnel);
            ScriptsList.AddHandler(PointerCaptureLostEvent, (_, _) => EndPress(), RoutingStrategies.Tunnel);
        }

        private ScriptsViewModel ViewModel => DataContext as ScriptsViewModel;

        private void OnPointerPressed(object sender, PointerPressedEventArgs e)
        {
            ResetPress();

            var point = e.GetCurrentPoint(ScriptsList);
            if (!point.Properties.IsLeftButtonPressed)
                return;

            // The handle only, and only while it is enabled - locked with the switch while the game runs unlinked.
            var grip = (e.Source as Visual)?.GetSelfAndVisualAncestors()
                .OfType<Border>()
                .FirstOrDefault(border => border.Classes.Contains("SMGrip"));
            if (grip is not { IsEffectivelyEnabled: true } || grip.DataContext is not ScriptRowViewModel row || !row.CanReorder)
                return;

            _press = e;
            _pressedRow = row;
            _pressOrigin = point.Position;
        }

        private void OnPointerMoved(object sender, PointerEventArgs e)
        {
            if (_press == null || _dragging)
                return;

            var position = e.GetPosition(ScriptsList);
            if (Math.Abs(position.X - _pressOrigin.X) < DragThreshold && Math.Abs(position.Y - _pressOrigin.Y) < DragThreshold)
                return;

            _ = BeginDragAsync(_press, _pressedRow);
        }

        private void EndPress()
        {
            if (!_dragging)
                ResetPress();
        }

        private void ResetPress()
        {
            _press = null;
            _pressedRow = null;
            _dragging = false;
        }

        private async Task BeginDragAsync(PointerPressedEventArgs press, ScriptRowViewModel row)
        {
            _dragging = true;
            row.IsDragged = true;

            try
            {
                var transfer = new DataTransfer();
                transfer.Add(DataTransferItem.Create(RowFormat, row));

                // DoDragDropAsync owns the transfer and disposes it when the operation ends.
                await DragDrop.DoDragDropAsync(press, transfer, DragDropEffects.Move);
            }
            catch (Exception exception)
            {
                // Nothing awaits this, so an exception would otherwise vanish and read as "dragging does nothing".
                ApplicationNotificationManager.Manager.ShowError("Scripts", $"Could not change the order: {exception.Message}");
            }
            finally
            {
                row.IsDragged = false;
                ClearIndicator();
                _autoScroll.Stop();
                ResetPress();
            }
        }

        private void OnDragOver(object sender, DragEventArgs e)
        {
            if (e.DataTransfer?.Contains(RowFormat) == true)
            {
                e.DragEffects = DragDropEffects.Move;
                ShowIndicator(e);
                _autoScroll.Update(e);
            }
            else
            {
                e.DragEffects = DragDropEffects.None;
                ClearIndicator();
            }

            e.Handled = true;
        }

        private void OnDragLeave(object sender, DragEventArgs e)
        {
            ClearIndicator();
            _autoScroll.Stop();
        }

        private void OnDrop(object sender, DragEventArgs e)
        {
            ClearIndicator();
            _autoScroll.Stop();
            e.Handled = true;

            var row = e.DataTransfer?.TryGetValue(RowFormat);
            var vm = ViewModel;
            if (row == null || vm == null)
                return;

            var from = vm.Scripts.IndexOf(row);
            if (from < 0)
                return;

            // The gap the pointer is over, then ObservableCollection.Move's terms: every gap past the row's own place is
            // one lower once the row is lifted out. Rows with a place come first, so this is also their index among those;
            // a gap among the rows that have none is the end, which MoveScript clamps to.
            var (target, after) = RowAt(e);
            var insertion = target == null ? vm.Scripts.Count : vm.Scripts.IndexOf(target) + (after ? 1 : 0);
            vm.MoveScript(row, insertion > from ? insertion - 1 : insertion);
        }

        /// <summary>
        /// The row the pointer is over and whether it is in its lower half - by geometry against each row's container,
        /// not by e.Source, which during a drag depends on how the drop target was resolved. Null past the last row.
        /// </summary>
        private (ScriptRowViewModel Row, bool After) RowAt(DragEventArgs e)
        {
            var vm = ViewModel;
            if (vm == null)
                return (null, false);

            for (var i = 0; i < vm.Scripts.Count; i++)
            {
                if (ScriptsList.ContainerFromIndex(i) is not Control container)
                    continue;

                var y = e.GetPosition(container).Y;
                if (y >= 0 && y < container.Bounds.Height)
                    return (vm.Scripts[i], y > container.Bounds.Height / 2);
            }

            return (null, false);
        }

        private void ShowIndicator(DragEventArgs e)
        {
            var vm = ViewModel;
            var (target, after) = RowAt(e);

            // Past the end, or over the bundled scripts that are not installed (they have no place, and sit last): the
            // line goes under the last row that has one, which is where such a drop lands.
            if (vm != null && (target == null || !target.HasPlace))
            {
                target = vm.Scripts.LastOrDefault(r => r.HasPlace);
                after = true;
            }

            if (target == null)
            {
                ClearIndicator();
                return;
            }

            if (_indicatorRow != null && _indicatorRow != target)
                _indicatorRow.ClearDropIndicator();

            _indicatorRow = target;
            target.DropBefore = !after;
            target.DropAfter = after;
        }

        private void ClearIndicator()
        {
            _indicatorRow?.ClearDropIndicator();
            _indicatorRow = null;
        }
    }
}
