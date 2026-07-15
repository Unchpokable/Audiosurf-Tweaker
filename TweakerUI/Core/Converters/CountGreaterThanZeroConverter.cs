using System;
using System.Globalization;
using Avalonia.Data.Converters;

namespace TweakerUI.Core.Converters
{
    // Drives the status bar's IsVisible - collapses the bar entirely when ActiveStatuses is empty,
    // the same "quiet by default" principle StatusService itself follows.
    internal class CountGreaterThanZeroConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return value is int count && count > 0;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotSupportedException();
        }
    }
}
