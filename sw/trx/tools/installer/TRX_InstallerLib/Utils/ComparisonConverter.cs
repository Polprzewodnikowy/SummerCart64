using System.Globalization;
using WD = System.Windows.Data;

namespace TRX_InstallerLib.Utils;

public class ComparisonConverter : WD.IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        return value.Equals(parameter);
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
    {
        return (bool)value ? parameter : WD.Binding.DoNothing;
    }
}
