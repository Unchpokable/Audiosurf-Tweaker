using System;
using System.Collections.Generic;
using Avalonia.Animation;
using Avalonia.Media;
using Avalonia.Media.Immutable;

namespace TweakerUI.Core
{
    // The only brush transition this app uses - Avalonia's own BrushTransition is not used anywhere
    // (Docs/Internal/avalonia-pitfalls.md, §5).
    //
    // BrushTransition interpolates A, R, G and B independently on straight (non-premultiplied) color.
    // What the screen shows is that color composited over whatever is behind the control, and as soon
    // as the two endpoints differ in BOTH alpha and color, the composited in-between frames leave the
    // line between the two on-screen endpoints: resting on Transparent (#00FFFFFF) and fading to a dark
    // hover flashes light gray, an opaque surface fading to a 15% accent tint swells into a much
    // stronger tint mid-way. The project used to dodge this with a hand-kept rule ("a transition may
    // move hue or alpha, never both") and zero-alpha "ghost" brushes; every pair that broke the rule
    // flickered (KnownBugs #6).
    //
    // Interpolating PREMULTIPLIED color fixes it for any pair: composited over any background b, a frame
    // is premul(t) + b * (1 - alpha(t)), and with premul and alpha both linear in t, so is the frame -
    // an exact crossfade of the two endpoints as they appear on screen. Brush.Opacity is folded into
    // alpha, so an Opacity attribute on a brush animates correctly too.
    public sealed class CrossfadeBrushTransition : InterpolatingTransitionBase<IBrush>
    {
        protected override IBrush Interpolate(double progress, IBrush from, IBrush to)
        {
            if (from is ILinearGradientBrush fromGradient && to is ILinearGradientBrush toGradient
                && fromGradient.GradientStops.Count == toGradient.GradientStops.Count)
            {
                return InterpolateGradient(progress, fromGradient, toGradient);
            }

            if (!TryGetColor(from, out var fromColor) || !TryGetColor(to, out var toColor))
                return progress < 0.5 ? from : to; // gradient vs solid, or anything exotic: snap mid-way, like BrushTransition

            return new ImmutableSolidColorBrush(Lerp(fromColor, toColor, progress));
        }

        // null (an unset Background) counts as fully transparent, so fading in from "nothing" works too.
        private static bool TryGetColor(IBrush brush, out Color color)
        {
            switch (brush)
            {
                case null:
                    color = Colors.Transparent;
                    return true;
                case ISolidColorBrush solid:
                    color = WithOpacity(solid.Color, solid.Opacity);
                    return true;
                default:
                    color = default;
                    return false;
            }
        }

        private static Color WithOpacity(Color color, double opacity) =>
            opacity >= 1 ? color : Color.FromArgb((byte)Math.Round(color.A * Math.Clamp(opacity, 0, 1)), color.R, color.G, color.B);

        private static Color Lerp(Color from, Color to, double t)
        {
            double fa = from.A / 255.0, ta = to.A / 255.0;
            var a = fa + (ta - fa) * t;
            if (a <= 0)
                return Color.FromArgb(0, to.R, to.G, to.B);

            byte Channel(byte f, byte c) => (byte)Math.Clamp(Math.Round((f * fa + (c * ta - f * fa) * t) / a), 0, 255);
            return Color.FromArgb((byte)Math.Round(a * 255), Channel(from.R, to.R), Channel(from.G, to.G), Channel(from.B, to.B));
        }

        // Only needed when a gradient Background re-resolves on a theme switch; keeps the target's geometry.
        private static IBrush InterpolateGradient(double progress, ILinearGradientBrush from, ILinearGradientBrush to)
        {
            var stops = new List<ImmutableGradientStop>(to.GradientStops.Count);
            for (var i = 0; i < to.GradientStops.Count; i++)
            {
                var f = from.GradientStops[i];
                var s = to.GradientStops[i];
                var color = Lerp(WithOpacity(f.Color, from.Opacity), WithOpacity(s.Color, to.Opacity), progress);
                stops.Add(new ImmutableGradientStop(f.Offset + (s.Offset - f.Offset) * progress, color));
            }

            return new ImmutableLinearGradientBrush(stops, 1, null, null, to.SpreadMethod, to.StartPoint, to.EndPoint);
        }
    }
}
