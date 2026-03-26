using System.Windows;
using TR2X_Installer.Installers;
using TRX_InstallerLib.Controls;
using TRX_InstallerLib.Installers;

namespace TR2X_Installer;

public partial class App : Application
{
    public App()
    {
        Current.MainWindow = new TRXInstallWindow(new List<IInstallSource>
        {
            new SteamInstallSource(),
            new GOGInstallSource(),
            new CDRomInstallSource(),
            new TR2XInstallSource(),
        });
        Current.MainWindow.Show();
    }
}
