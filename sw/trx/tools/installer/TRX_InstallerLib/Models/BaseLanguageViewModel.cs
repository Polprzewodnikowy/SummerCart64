using TRX_InstallerLib.Utils;

namespace TRX_InstallerLib.Models;

public class BaseLanguageViewModel : BaseNotifyPropertyChanged
{
    public static Dictionary<string, string> ViewText
    {
        get => Language.Instance.Controls ?? new();
    }
}
