using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace TRX_InstallerLib.Utils;

public abstract class BaseNotifyPropertyChanged : INotifyPropertyChanged
{
    public event PropertyChangedEventHandler? PropertyChanged;

    public void NotifyPropertyChanged([CallerMemberName] string propertyName = "")
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
}
