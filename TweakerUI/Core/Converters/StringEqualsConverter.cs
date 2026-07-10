using System;
using System.Globalization;
using Avalonia.Data.Converters;

namespace TweakerUI.Core.Converters
{
    // Drives the HSV/HSL/RGB tab "selected" look and the corresponding slider group's visibility -
    // compares the bound value against ConverterParameter, e.g. {Binding ColorTab, ConverterParameter=HSV}.
    internal class StringEqualsConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return value?.ToString() == parameter?.ToString();
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotSupportedException();
        }
    }
}
