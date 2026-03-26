using System.IO;
using System.Text.RegularExpressions;
using TRX_InstallerLib.Installers;
using TRX_InstallerLib.Utils;

namespace TR1X_Installer.Installers;

public class TombATIInstallSource : BaseInstallSource
{
    public override IEnumerable<string> DirectoriesToTry
    {
        get
        {
            yield return "C:\\TOMBATI";
            foreach (var path in InstallUtils.GetDesktopShortcutDirectories())
            {
                yield return path;
            }
            foreach (var path in new SteamInstallSource().DirectoriesToTry)
            {
                yield return path;
            }
        }
    }

    public override bool IsImportingSavesSupported => true;
    public override string SourceName => "TombATI";

    public override async Task CopyOriginalGameFiles(
        string sourceDirectory,
        string targetDirectory,
        IProgress<InstallProgress> progress,
        bool importSaves
    )
    {
        var filterRegex = new Regex(importSaves ? @"(data|fmv|music)[\\/]|save.*\.\d+\b" : @"(data|fmv|music)[\\/]", RegexOptions.IgnoreCase);
        await InstallUtils.CopyDirectoryTree(
            sourceDirectory,
            targetDirectory,
            progress,
            file => filterRegex.IsMatch(file),
            path => ConvertTargetPath(path)
        );
    }

    public override bool IsDownloadingMusicNeeded(string sourceDirectory)
    {
        return !Directory.Exists(Path.Combine(sourceDirectory, "music"));
    }

    public override bool IsDownloadingExpansionNeeded(string sourceDirectory)
    {
        return !File.Exists(Path.Combine(sourceDirectory, "data", "cat.phd"));
    }

    public override bool IsGameFound(string sourceDirectory)
    {
        return Directory.Exists(Path.Combine(sourceDirectory, "DATA"))
            && Directory.Exists(Path.Combine(sourceDirectory, "FMV"))
            && File.Exists(Path.Combine(sourceDirectory, "TombATI.exe"));
    }
}
