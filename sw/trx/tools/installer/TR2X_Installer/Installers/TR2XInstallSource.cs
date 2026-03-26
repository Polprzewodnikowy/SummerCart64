using System.IO;
using System.Text.RegularExpressions;
using TRX_InstallerLib.Installers;
using TRX_InstallerLib.Utils;

namespace TR2X_Installer.Installers;

public class TR2XInstallSource : GenericInstallSource
{
    public override IEnumerable<string> DirectoriesToTry
    {
        get
        {
            var previousPath = InstallUtils.GetPreviousInstallationPath();
            if (previousPath is not null)
            {
                yield return previousPath;
            }

            foreach (var path in InstallUtils.GetDesktopShortcutDirectories())
            {
                yield return path;
            }
        }
    }

    public override string SuggestedInstallationDirectory
    {
        get
        {
            return InstallUtils.GetPreviousInstallationPath() ?? base.SuggestedInstallationDirectory;
        }
    }

    public override bool IsImportingSavesSupported => true;
    public override string SourceName => "TR2X";

    public override async Task CopyOriginalGameFiles(
        string sourceDirectory,
        string targetDirectory,
        IProgress<InstallProgress> progress,
        bool importSaves
    )
    {
        var filterRegex = new Regex(importSaves ? @"(audio|data|fmv|music|saves)[\\/]|save.*\.\d+" : @"(audio|data|fmv|music)[\\/]", RegexOptions.IgnoreCase);
        await InstallUtils.CopyDirectoryTree(
            sourceDirectory,
            targetDirectory,
            progress,
            file => filterRegex.IsMatch(file)
        );
    }

    public override bool IsDownloadingExpansionNeeded(string sourceDirectory)
    {
        return !File.Exists(Path.Combine(sourceDirectory, "data", "title_gm.tr2"));
    }

    public override bool IsGameFound(string sourceDirectory)
    {
        return File.Exists(Path.Combine(sourceDirectory, "TRX.exe"))
            || File.Exists(Path.Combine(sourceDirectory, "TR2X.exe"));
    }
}
