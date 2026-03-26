using Microsoft.Win32;
using System.IO;

namespace TR1X_Installer.Installers;

public class SteamInstallSource : GOGInstallSource
{
    public override IEnumerable<string> DirectoriesToTry
    {
        get
        {
            yield return @"C:\Program Files (x86)\Steam\steamapps\common\Tomb Raider (I)";

            using var key = Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam");
            if (key is not null)
            {
                var value = key.GetValue("SteamPath")?.ToString();
                if (value is not null)
                {
                    yield return Path.Combine(value, @"steamapps\common\Tomb Raider (I)");
                }
            }
        }
    }

    public override string SourceName => "Steam";
}
