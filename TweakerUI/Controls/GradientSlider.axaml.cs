using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Data;
using Avalonia.Input;
using Avalonia.Media;

namespace TweakerUI.Controls
{
    // The mockup's sliders are native HTML <input type="range"> with a CSS background painted
    // straight onto the track, which has no equivalent via Avalonia's stock Slider template short of
    // a full retemplate. A small self-contained drag control matches the mockup's exact look (full-width
    // gradient track, circular thumb) with the same pointer-capture pattern as HsvWheel.
    public partial class GradientSlider : UserControl
    {
        public static readonly StyledProperty<double> MinimumProperty =
            AvaloniaProperty.Register<GradientSlider, double>(nameof(Minimum));

        public static readonly StyledProperty<double> MaximumProperty =
            AvaloniaProperty.Register<GradientSlider, double>(nameof(Maximum), 100);

        public static readonly StyledProperty<double> ValueProperty =
            AvaloniaProperty.Register<GradientSlider, double>(nameof(Value), defaultBindingMode: BindingMode.TwoWay);

        public static readonly StyledProperty<IBrush> TrackBrushProperty =
            AvaloniaProperty.Register<GradientSlider, IBrush>(nameof(TrackBrush));

        public double Minimum
        {
            get => GetValue(MinimumProperty);
            set => SetValue(MinimumProperty, value);
        }

        public double Maximum
        {
            get => GetValue(MaximumProperty);
            set => SetValue(MaximumProperty, value);
        }

        public double Value
        {
            get => GetValue(ValueProperty);
            set => SetValue(ValueProperty, value);
        }

        public IBrush TrackBrush
        {
            get => GetValue(TrackBrushProperty);
            set => SetValue(TrackBrushProperty, value);
        }

        private bool _dragging;

        static GradientSlider()
        {
            ValueProperty.Changed.AddClassHandler<GradientSlider>((s, _) => s.UpdateThumb());
            MinimumProperty.Changed.AddClassHandler<GradientSlider>((s, _) => s.UpdateThumb());
            MaximumProperty.Changed.AddClassHandler<GradientSlider>((s, _) => s.UpdateThumb());
            TrackBrushProperty.Changed.AddClassHandler<GradientSlider>((s, _) => s.UpdateTrackBrush());
        }

        public GradientSlider()
        {
            InitializeComponent();
            UpdateTrackBrush();
            SizeChanged += (_, _) => UpdateThumb();
        }

        private void UpdateTrackBrush() => Track.Background = TrackBrush;

        private void UpdateThumb()
        {
            var width = Bounds.Width;
            if (width <= 0 || Maximum <= Minimum) return;
            var fraction = Math.Clamp((Value - Minimum) / (Maximum - Minimum), 0, 1);
            var usableWidth = Math.Max(0, width - Thumb.Width);
            Thumb.Margin = new Thickness(fraction * usableWidth, 0, 0, 0);
        }

        private void OnPointerPressed(object sender, PointerPressedEventArgs e)
        {
            Focus();
            _dragging = true;
            e.Pointer.Capture(RootPanel);
            UpdateFromPointer(e.GetPosition(RootPanel));
            e.Handled = true;
        }

        protected override void OnPointerMoved(PointerEventArgs e)
        {
            base.OnPointerMoved(e);
            if (_dragging)
                UpdateFromPointer(e.GetPosition(RootPanel));
        }

        protected override void OnPointerReleased(PointerReleasedEventArgs e)
        {
            base.OnPointerReleased(e);
            if (_dragging)
                e.Pointer.Capture(null);
            _dragging = false;
        }

        private void UpdateFromPointer(Point p)
        {
            var width = Bounds.Width;
            if (width <= 0) return;
            var fraction = Math.Clamp(p.X / width, 0, 1);
            Value = Math.Round(Minimum + fraction * (Maximum - Minimum), 1);
        }
    }
}
