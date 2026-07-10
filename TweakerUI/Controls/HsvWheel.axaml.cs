using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using SkiaSharp;
using TweakerUI.Core;

namespace TweakerUI.Controls
{
    // Faithful port of the "Palette Designer" mockup's hue wheel + saturation/value square: a fixed
    // 220x220 circular hue ring (SkiaSharp sweep gradient, since Avalonia has no native conic-gradient
    // brush) with an inset SV square, both pointer-draggable. All position math (handle placement, angle
    // <-> hue, square-local S/V) mirrors the mockup's JS 1:1 rather than being reinvented.
    public partial class HsvWheel : UserControl
    {
        public static readonly StyledProperty<double> HueProperty =
            AvaloniaProperty.Register<HsvWheel, double>(nameof(Hue), defaultBindingMode: Avalonia.Data.BindingMode.TwoWay);

        public static readonly StyledProperty<double> SaturationProperty =
            AvaloniaProperty.Register<HsvWheel, double>(nameof(Saturation), 100, defaultBindingMode: Avalonia.Data.BindingMode.TwoWay);

        public static readonly StyledProperty<double> ValueProperty =
            AvaloniaProperty.Register<HsvWheel, double>(nameof(Value), 100, defaultBindingMode: Avalonia.Data.BindingMode.TwoWay);

        public double Hue
        {
            get => GetValue(HueProperty);
            set => SetValue(HueProperty, value);
        }

        public double Saturation
        {
            get => GetValue(SaturationProperty);
            set => SetValue(SaturationProperty, value);
        }

        public double Value
        {
            get => GetValue(ValueProperty);
            set => SetValue(ValueProperty, value);
        }

        // Scaled down from the mockup's 220px wheel to 160px - same ratios (SquareOffset/SquareSize
        // are 26%/48% of the wheel diameter, HandleOrbitRadius is diameter/2 minus a fixed 14px inset
        // for the handle), just smaller, to stop the color editor card from dominating the window height.
        private const double WheelSize = 160;
        private const double Center = WheelSize / 2;
        private const double HandleOrbitRadius = Center - 14;
        private const double SquareOffset = WheelSize * 0.26;
        private const double SquareSize = WheelSize * 0.48;

        private bool _draggingHue;
        private bool _draggingSv;
        private static Bitmap _wheelBitmapCache;

        static HsvWheel()
        {
            HueProperty.Changed.AddClassHandler<HsvWheel>((w, _) =>
            {
                w.UpdateHueHandle();
                w.UpdatePureHueLayer();
            });
            SaturationProperty.Changed.AddClassHandler<HsvWheel>((w, _) => w.UpdateSvHandle());
            ValueProperty.Changed.AddClassHandler<HsvWheel>((w, _) => w.UpdateSvHandle());
        }

        public HsvWheel()
        {
            InitializeComponent();
            WheelEllipse.Fill = new ImageBrush(GetOrCreateWheelBitmap()) { Stretch = Stretch.Fill };
            UpdateHueHandle();
            UpdatePureHueLayer();
            UpdateSvHandle();
        }

        private static Bitmap GetOrCreateWheelBitmap()
        {
            return _wheelBitmapCache ??= CreateWheelBitmap();
        }

        private static Bitmap CreateWheelBitmap()
        {
            const int size = 220;
            using var bitmap = new SKBitmap(size, size, SKColorType.Bgra8888, SKAlphaType.Premul);
            using (var canvas = new SKCanvas(bitmap))
            {
                canvas.Clear(SKColors.Transparent);
                var center = new SKPoint(size / 2f, size / 2f);
                // Same 6-stop rainbow the H slider track uses, so wheel and sliders read as one system.
                var colors = new[]
                {
                    new SKColor(0xFF, 0x00, 0x00), new SKColor(0xFF, 0xFF, 0x00), new SKColor(0x00, 0xFF, 0x00),
                    new SKColor(0x00, 0xFF, 0xFF), new SKColor(0x00, 0x00, 0xFF), new SKColor(0xFF, 0x00, 0xFF),
                    new SKColor(0xFF, 0x00, 0x00),
                };
                using var shader = SKShader.CreateSweepGradient(center, colors);
                using var paint = new SKPaint { Shader = shader, IsAntialias = true };
                canvas.DrawCircle(center, size / 2f, paint);
            }
            return bitmap.ToAvaloniaBitmap();
        }

        private void UpdateHueHandle()
        {
            var angleRad = (Hue + 90) * Math.PI / 180;
            var x = Center + HandleOrbitRadius * Math.Sin(angleRad);
            var y = Center - HandleOrbitRadius * Math.Cos(angleRad);
            HueHandle.Margin = new Thickness(x - HueHandle.Width / 2, y - HueHandle.Height / 2, 0, 0);
        }

        private void UpdatePureHueLayer()
        {
            PureHueLayer.Background = new SolidColorBrush(ColorMath.HsvToColor(Hue, 100, 100));
        }

        private void UpdateSvHandle()
        {
            var x = SquareOffset + Saturation / 100.0 * SquareSize;
            var y = SquareOffset + (100 - Value) / 100.0 * SquareSize;
            SvHandle.Margin = new Thickness(x - SvHandle.Width / 2, y - SvHandle.Height / 2, 0, 0);
        }

        private void OnWheelPointerPressed(object sender, PointerPressedEventArgs e)
        {
            Focus();
            _draggingHue = true;
            e.Pointer.Capture(this);
            UpdateHueFromPointer(e.GetPosition(this));
            e.Handled = true;
        }

        private void OnSquarePointerPressed(object sender, PointerPressedEventArgs e)
        {
            Focus();
            _draggingSv = true;
            e.Pointer.Capture(this);
            UpdateSvFromPointer(e.GetPosition(this));
            e.Handled = true;
        }

        protected override void OnPointerMoved(PointerEventArgs e)
        {
            base.OnPointerMoved(e);
            if (_draggingHue)
                UpdateHueFromPointer(e.GetPosition(this));
            else if (_draggingSv)
                UpdateSvFromPointer(e.GetPosition(this));
        }

        protected override void OnPointerReleased(PointerReleasedEventArgs e)
        {
            base.OnPointerReleased(e);
            if (_draggingHue || _draggingSv)
                e.Pointer.Capture(null);
            _draggingHue = false;
            _draggingSv = false;
        }

        private void UpdateHueFromPointer(Point p)
        {
            var dx = p.X - Center;
            var dy = p.Y - Center;
            var angleDeg = Math.Atan2(dx, -dy) * 180 / Math.PI;
            if (angleDeg < 0) angleDeg += 360;
            Hue = Math.Round((angleDeg - 90 + 360) % 360, 1);
        }

        private void UpdateSvFromPointer(Point p)
        {
            var x = Math.Clamp((p.X - SquareOffset) / SquareSize, 0, 1);
            var y = Math.Clamp((p.Y - SquareOffset) / SquareSize, 0, 1);
            Saturation = Math.Round(x * 100, 1);
            Value = Math.Round((1 - y) * 100, 1);
        }
    }
}
