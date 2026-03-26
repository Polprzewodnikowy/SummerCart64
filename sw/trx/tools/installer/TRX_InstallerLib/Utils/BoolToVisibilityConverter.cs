using System.Globalization;
using System.Windows;
using System.Windows.Data;

namespace TRX_InstallerLib.Utils;

[ValueConversion(typeof(bool), typeof(Visibility))]
public class BoolToVisibilityConverter : IValueConverter
{
    public BoolToVisibilityConverter()
    {
        FalseValue = Visibility.Hidden;
        TrueValue = Visibility.Visible;
    }

    public Visibility FalseValue { get; set; }
    public Visibility TrueValue { get; set; }

    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        return (bool)value ? TrueValue : FalseValue;
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
    {
        throw new NotImplementedException();
    }
}
