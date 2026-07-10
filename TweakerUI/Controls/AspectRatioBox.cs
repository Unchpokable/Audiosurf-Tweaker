using Avalonia;
using Avalonia.Controls;

namespace TweakerUI.Controls
{
    // Avalonia has no CSS-style "aspect-ratio" layout primitive. Stretching the app window used to
    // crop screenshot previews awkwardly because their container had a fixed pixel Height while its
    // width followed the column - the two only matched the image's own aspect by coincidence at one
    // particular width. This Decorator instead derives Height from whatever width it's given, so the
    // child always gets a box of the requested Ratio (default 16:9) regardless of window size.
    public class AspectRatioBox : Decorator
    {
        public static readonly StyledProperty<double> RatioProperty =
            AvaloniaProperty.Register<AspectRatioBox, double>(nameof(Ratio), 16.0 / 9.0);

        public double Ratio
        {
            get => GetValue(RatioProperty);
            set => SetValue(RatioProperty, value);
        }

        static AspectRatioBox()
        {
            AffectsMeasure<AspectRatioBox>(RatioProperty);
        }

        protected override Size MeasureOverride(Size availableSize)
        {
            var width = double.IsInfinity(availableSize.Width) ? 0 : availableSize.Width;
            var height = Ratio > 0 ? width / Ratio : availableSize.Height;

            Child?.Measure(new Size(width, height));
            return new Size(width, height);
        }

        protected override Size ArrangeOverride(Size finalSize)
        {
            var height = Ratio > 0 ? finalSize.Width / Ratio : finalSize.Height;

            Child?.Arrange(new Rect(0, 0, finalSize.Width, height));
            return new Size(finalSize.Width, height);
        }
    }
}
