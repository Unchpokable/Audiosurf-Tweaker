using System;
using System.Globalization;
using System.Windows;
using System.Windows.Data;
using System.Windows.Media;

namespace SkinChangerRestyle.Core.Converters
{
    [ValueConversion(typeof(bool), typeof(Color))]
    public class InversedBooleanToColorConverter : IValueConverter
    {
        private static Color _redColor = Colors.Red;
        private static Color _greenColor = Colors.LimeGreen;

        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            var val = value != null && (bool)value;

            if (val)
                return _redColor;
            return _greenColor;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            try
            {
                if (value != null)
                {
                    var colorValue = (Color)value;
                    if (colorValue == _redColor)
                        return true;

                    if (colorValue == _greenColor)
                        return false;
                }

                return DependencyProperty.UnsetValue;
            }
            catch
            {
                return DependencyProperty.UnsetValue;
            }
        }
    }
}
