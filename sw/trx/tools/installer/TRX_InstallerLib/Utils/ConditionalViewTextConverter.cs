using System.Globalization;
using TRX_InstallerLib.Models;

namespace TRX_InstallerLib.Utils;

public class ConditionalViewTextConverter : ConditionalMarkupConverter
{
    public override object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        string result = base.Convert(value, targetType, parameter, culture).ToString()!;
        return result.Length == 0 ? string.Empty : Language.Instance.Controls![result];
    }
}
