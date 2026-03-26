using Microsoft.Win32;
using System.IO;
using System.Text.RegularExpressions;

namespace TR2X_Installer.Installers;

public class GOGInstallSource : GenericInstallSource
{
    public override IEnumerable<string> DirectoriesToTry
    {
        get
        {
            yield return @"C:\Program Files (x86)\GOG Galaxy\Games\Tomb Raider 2";

            using var key = Registry.ClassesRoot.OpenSubKey(@"goggalaxy\shell\open\command");
            if (key is not null)
            {
                var value = key.GetValue("")?.ToString();
                if (value is not null && new Regex(@"""(?<path>[^""]+)""").Match(value) is { Success: true } match)
                {
                    yield return Path.Combine(Path.GetDirectoryName(match.Groups["path"].Value)!, @"Games\Tomb Raider 2");
                }
            }
        }
    }

    public override bool IsImportingSavesSupported => true;
    public override string SourceName => "GOG";

    public override bool IsGameFound(string sourceDirectory)
    {
        return File.Exists(Path.Combine(sourceDirectory, "tomb2.exe"))
            && File.Exists(Path.Combine(sourceDirectory, "data", "wall.tr2"))
            && File.Exists(Path.Combine(sourceDirectory, "data", "main.sfx"));
    }
}
