using System;
using System.Globalization;
using System.Windows.Data;

namespace Updater.Core.Converters
{
    public class FileSizeToStringConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is long fileSize)
            {
                return FormatFileSize(fileSize);
            }

            return value;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotSupportedException();
        }

        private string FormatFileSize(long fileSize)
        {
            const int byteConversion = 1024;
            string[] sizeSuffixes = { "bytes", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB" };

            if (fileSize == 0)
            {
                return "0 bytes";
            }

            var absoluteFileSize = Math.Abs(fileSize);
            var order = System.Convert.ToInt32(Math.Floor(Math.Log(absoluteFileSize, byteConversion)));
            var suffixIndex = Math.Min(order, sizeSuffixes.Length - 1);

            double adjustedSize = fileSize / Math.Pow(byteConversion, suffixIndex);
            return $"{adjustedSize:0.0} {sizeSuffixes[suffixIndex]}";
        }
    }
}
