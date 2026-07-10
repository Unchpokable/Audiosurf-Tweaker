using System;
using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.VisualTree;

namespace TweakerUI.Controls
{
    // Vertically stacks children with spacing that adapts to the available height, instead of a
    // fixed gap: shrinks toward MinSpacing when content doesn't quite fit (so a ScrollViewer wrapping
    // this only kicks in once spacing is already minimal, not the instant content is a pixel too
    // tall), and grows up to MaxSpacing to fill leftover space instead of leaving a dead gap at the
    // bottom of a tall window.
    //
    // A ScrollViewer always offers PositiveInfinity for the scrolling axis during Measure, which is
    // useless for "how much room do I actually have" - so instead of trusting availableSize, this
    // walks up to its hosting ScrollViewer (if any) and tracks its real Bounds directly.
    public class AdaptiveStackPanel : Panel
    {
        public static readonly StyledProperty<double> MinSpacingProperty =
            AvaloniaProperty.Register<AdaptiveStackPanel, double>(nameof(MinSpacing), 8);

        public static readonly StyledProperty<double> MaxSpacingProperty =
            AvaloniaProperty.Register<AdaptiveStackPanel, double>(nameof(MaxSpacing), 32);

        public double MinSpacing
        {
            get => GetValue(MinSpacingProperty);
            set => SetValue(MinSpacingProperty, value);
        }

        public double MaxSpacing
        {
            get => GetValue(MaxSpacingProperty);
            set => SetValue(MaxSpacingProperty, value);
        }

        private double _resolvedSpacing;
        private double _viewportHeight;
        private ScrollViewer _scrollViewer;

        static AdaptiveStackPanel()
        {
            AffectsMeasure<AdaptiveStackPanel>(MinSpacingProperty, MaxSpacingProperty);
        }

        protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnAttachedToVisualTree(e);
            _scrollViewer = this.GetVisualAncestors().OfType<ScrollViewer>().FirstOrDefault();
            if (_scrollViewer != null)
                _scrollViewer.PropertyChanged += OnScrollViewerPropertyChanged;
        }

        protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnDetachedFromVisualTree(e);
            if (_scrollViewer != null)
                _scrollViewer.PropertyChanged -= OnScrollViewerPropertyChanged;
            _scrollViewer = null;
        }

        private void OnScrollViewerPropertyChanged(object sender, AvaloniaPropertyChangedEventArgs e)
        {
            if (e.Property != BoundsProperty)
                return;

            var height = ((Rect)e.NewValue).Height;
            if (Math.Abs(_viewportHeight - height) < 0.5)
                return;
            _viewportHeight = height;
            InvalidateMeasure();
        }

        protected override Size MeasureOverride(Size availableSize)
        {
            var visible = Children.Where(c => c.IsVisible).ToList();
            double contentHeight = 0;
            double maxWidth = 0;

            foreach (var child in visible)
            {
                child.Measure(new Size(availableSize.Width, double.PositiveInfinity));
                contentHeight += child.DesiredSize.Height;
                maxWidth = Math.Max(maxWidth, child.DesiredSize.Width);
            }

            var gaps = Math.Max(0, visible.Count - 1);

            var target = availableSize.Height;
            if (double.IsInfinity(target))
                target = _viewportHeight > 0 ? _viewportHeight : contentHeight;

            _resolvedSpacing = gaps == 0 ? 0 : Math.Clamp((target - contentHeight) / gaps, MinSpacing, MaxSpacing);

            return new Size(maxWidth, contentHeight + _resolvedSpacing * gaps);
        }

        protected override Size ArrangeOverride(Size finalSize)
        {
            double y = 0;
            foreach (var child in Children.Where(c => c.IsVisible))
            {
                var height = child.DesiredSize.Height;
                child.Arrange(new Rect(0, y, finalSize.Width, height));
                y += height + _resolvedSpacing;
            }
            return finalSize;
        }
    }
}
