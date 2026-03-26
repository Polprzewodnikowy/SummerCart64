using System.Windows;
using TR1X_Installer.Installers;
using TRX_InstallerLib.Controls;
using TRX_InstallerLib.Installers;

namespace TR1X_Installer;

public partial class App : Application
{
    public App()
    {
        Current.MainWindow = new TRXInstallWindow(new List<IInstallSource>
        {
            new SteamInstallSource(),
            new GOGInstallSource(),
            new TombATIInstallSource(),
            new CDRomInstallSource(),
            new TR1XInstallSource(),
        });
        Current.MainWindow.Show();
    }
}
