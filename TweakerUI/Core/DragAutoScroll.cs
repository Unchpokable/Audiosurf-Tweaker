using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Threading;

namespace TweakerUI.Core
{
    /// <summary>
    /// Scrolls a list while something is dragged near its top or bottom edge - otherwise taking a row to the far end
    /// of a long list means dropping, scrolling and dragging again. Shared by every drag-to-reorder list (Quick
    /// Player's queue, the Scripts tab).
    ///
    /// Driven by a timer rather than by DragOver, because the pointer sitting still at the edge raises no further
    /// events. Call <see cref="Update"/> from DragOver and <see cref="Stop"/> when the drag leaves or ends.
    /// </summary>
    public sealed class DragAutoScroll
    {
        private const double Margin = 36;
        private const double Step = 12;

        private readonly ScrollViewer _scroll;
        private DispatcherTimer _timer;
        private double _delta;

        public DragAutoScroll(ScrollViewer scroll)
        {
            _scroll = scroll;
        }

        public void Update(DragEventArgs e)
        {
            var height = _scroll.Bounds.Height;
            var y = e.GetPosition(_scroll).Y;

            if (y < Margin)
                _delta = -Step;
            else if (y > height - Margin)
                _delta = Step;
            else
                _delta = 0;

            if (_delta == 0)
            {
                Stop();
                return;
            }

            if (_timer != null)
                return;

            _timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(30) };
            _timer.Tick += OnTick;
            _timer.Start();
        }

        public void Stop()
        {
            if (_timer == null)
                return;

            _timer.Stop();
            _timer.Tick -= OnTick;
            _timer = null;
            _delta = 0;
        }

        private void OnTick(object sender, EventArgs e)
        {
            var offset = _scroll.Offset;
            var max = Math.Max(0, _scroll.Extent.Height - _scroll.Viewport.Height);
            _scroll.Offset = new Vector(offset.X, Math.Clamp(offset.Y + _delta, 0, max));
        }
    }
}
