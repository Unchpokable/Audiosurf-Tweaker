using Avalonia;
using Avalonia.Media;
using CommunityToolkit.Mvvm.ComponentModel;
using TweakerUI.Core;

namespace TweakerUI.Models
{
    // Live-editing state for one of the palette's 5 color roles. HSV is the source of truth while
    // editing (not RGB) so dragging hue at zero saturation, for instance, doesn't lose the hue the
    // instant it's committed - same reasoning as the "Palette Designer" mockup's state.swatchColors.
    public partial class SwatchEditState : ObservableObject
    {
        public SwatchEditState(ASColors role, string name)
        {
            Role = role;
            Name = name;
        }

        public ASColors Role { get; }
        public string Name { get; }

        [ObservableProperty]
        private double hue;

        [ObservableProperty]
        private double saturation = 100;

        [ObservableProperty]
        private double value = 100;

        [ObservableProperty]
        private bool isSelected;

        public void SetFromColor(Color color)
        {
            var hsv = ColorMath.ColorToHsv(color);
            Hue = hsv.H;
            Saturation = hsv.S;
            Value = hsv.V;
        }

        public Color Color => ColorMath.HsvToColor(Hue, Saturation, Value);
        public IBrush Brush => new SolidColorBrush(Color);

        public string Hex
        {
            get => ColorMath.ToHex(Color);
            set
            {
                if (ColorMath.TryParseHex(value, out var color))
                    SetFromColor(color);
            }
        }

        private static byte ClampByte(double v) => (byte)System.Math.Clamp(System.Math.Round(v), 0, 255);

        public double R
        {
            get => Color.R;
            set => SetFromColor(Color.FromRgb(ClampByte(value), Color.G, Color.B));
        }

        public double G
        {
            get => Color.G;
            set => SetFromColor(Color.FromRgb(Color.R, ClampByte(value), Color.B));
        }

        public double B
        {
            get => Color.B;
            set => SetFromColor(Color.FromRgb(Color.R, Color.G, ClampByte(value)));
        }

        public double HslHue
        {
            get => ColorMath.HsvToHsl(Hue, Saturation, Value).H;
            set => ApplyHsl(value, HslSaturation, HslLightness);
        }

        public double HslSaturation
        {
            get => ColorMath.HsvToHsl(Hue, Saturation, Value).S;
            set => ApplyHsl(HslHue, value, HslLightness);
        }

        public double HslLightness
        {
            get => ColorMath.HsvToHsl(Hue, Saturation, Value).L;
            set => ApplyHsl(HslHue, HslSaturation, value);
        }

        private void ApplyHsl(double h, double s, double l)
        {
            var hsv = ColorMath.HslToHsv(h, s, l);
            Hue = hsv.H;
            Saturation = hsv.S;
            Value = hsv.V;
        }

        // Static rainbow track, shared by the HSV-H and HSL-H sliders - same 6-stop list the hue
        // wheel bitmap uses, so the whole picker reads as one continuous rainbow.
        private static readonly IBrush HueTrack = CreateHueTrackBrush();

        private static IBrush CreateHueTrackBrush()
        {
            var brush = new LinearGradientBrush
            {
                StartPoint = new RelativePoint(0, 0.5, RelativeUnit.Relative),
                EndPoint = new RelativePoint(1, 0.5, RelativeUnit.Relative),
            };
            Color[] stops = { Colors.Red, Colors.Yellow, Colors.Lime, Colors.Cyan, Colors.Blue, Colors.Magenta, Colors.Red };
            for (var i = 0; i < stops.Length; i++)
                brush.GradientStops.Add(new GradientStop(stops[i], i / (double)(stops.Length - 1)));
            return brush;
        }

        private static IBrush TwoStop(Color from, Color to)
        {
            var brush = new LinearGradientBrush
            {
                StartPoint = new RelativePoint(0, 0.5, RelativeUnit.Relative),
                EndPoint = new RelativePoint(1, 0.5, RelativeUnit.Relative),
            };
            brush.GradientStops.Add(new GradientStop(from, 0));
            brush.GradientStops.Add(new GradientStop(to, 1));
            return brush;
        }

        public IBrush HueTrackBrush => HueTrack;
        public IBrush SaturationTrackBrush => TwoStop(ColorMath.HsvToColor(Hue, 0, Value), ColorMath.HsvToColor(Hue, 100, Value));
        public IBrush ValueTrackBrush => TwoStop(Colors.Black, ColorMath.HsvToColor(Hue, Saturation, 100));

        public IBrush HslSaturationTrackBrush
        {
            get
            {
                var l = HslLightness;
                var lowHsv = ColorMath.HslToHsv(Hue, 0, l);
                var highHsv = ColorMath.HslToHsv(Hue, 100, l);
                return TwoStop(ColorMath.HsvToColor(lowHsv.H, lowHsv.S, lowHsv.V), ColorMath.HsvToColor(highHsv.H, highHsv.S, highHsv.V));
            }
        }

        public IBrush HslLightnessTrackBrush
        {
            get
            {
                var mid = ColorMath.HslToHsv(Hue, HslSaturation, 50);
                var brush = new LinearGradientBrush
                {
                    StartPoint = new RelativePoint(0, 0.5, RelativeUnit.Relative),
                    EndPoint = new RelativePoint(1, 0.5, RelativeUnit.Relative),
                };
                brush.GradientStops.Add(new GradientStop(Colors.Black, 0));
                brush.GradientStops.Add(new GradientStop(ColorMath.HsvToColor(mid.H, mid.S, mid.V), 0.5));
                brush.GradientStops.Add(new GradientStop(Colors.White, 1));
                return brush;
            }
        }

        public IBrush RTrackBrush => TwoStop(Color.FromRgb(0, Color.G, Color.B), Color.FromRgb(255, Color.G, Color.B));
        public IBrush GTrackBrush => TwoStop(Color.FromRgb(Color.R, 0, Color.B), Color.FromRgb(Color.R, 255, Color.B));
        public IBrush BTrackBrush => TwoStop(Color.FromRgb(Color.R, Color.G, 0), Color.FromRgb(Color.R, Color.G, 255));

        partial void OnHueChanged(double value) => RaiseDerivedChanged();
        partial void OnSaturationChanged(double value) => RaiseDerivedChanged();
        partial void OnValueChanged(double value) => RaiseDerivedChanged();

        private void RaiseDerivedChanged()
        {
            OnPropertyChanged(nameof(Color));
            OnPropertyChanged(nameof(Brush));
            OnPropertyChanged(nameof(Hex));
            OnPropertyChanged(nameof(R));
            OnPropertyChanged(nameof(G));
            OnPropertyChanged(nameof(B));
            OnPropertyChanged(nameof(HslHue));
            OnPropertyChanged(nameof(HslSaturation));
            OnPropertyChanged(nameof(HslLightness));
            OnPropertyChanged(nameof(SaturationTrackBrush));
            OnPropertyChanged(nameof(ValueTrackBrush));
            OnPropertyChanged(nameof(HslSaturationTrackBrush));
            OnPropertyChanged(nameof(HslLightnessTrackBrush));
            OnPropertyChanged(nameof(RTrackBrush));
            OnPropertyChanged(nameof(GTrackBrush));
            OnPropertyChanged(nameof(BTrackBrush));
        }
    }
}
