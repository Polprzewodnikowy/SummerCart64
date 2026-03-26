using System.Globalization;
using System.Windows.Data;
using System.Windows.Markup;

namespace TRX_InstallerLib.Utils;

public class ConditionalMarkupConverter : MarkupExtension, IValueConverter
{
    public object FalseValue { get; set; } = new();
    public object TrueValue { get; set; } = new();

    public virtual object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        return value is true ? TrueValue : FalseValue;
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
    {
        throw new NotSupportedException();
    }

    public override object ProvideValue(IServiceProvider serviceProvider)
    {
        return this;
    }
}
