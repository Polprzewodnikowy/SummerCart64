using System.Windows;
using TRX_InstallerLib.Installers;
using TRX_InstallerLib.Models;

namespace TRX_InstallerLib.Controls;

public partial class TRXInstallWindow : Window
{
    public TRXInstallWindow(IEnumerable<IInstallSource> installSources)
    {
        InitializeComponent();
        DataContext = new MainWindowViewModel(installSources);
    }
}
