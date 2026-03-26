using System.IO;
using System.Text.RegularExpressions;
using TRX_InstallerLib.Installers;
using TRX_InstallerLib.Utils;

namespace TR2X_Installer.Installers;

public abstract class GenericInstallSource : BaseInstallSource
{
    private static readonly Dictionary<string, List<string>> _targetFiles = new()
    {
        ["data"] = new() { ".tr2", ".sfx", ".pcx" },
        ["fmv"] = new() { ".*" },
        ["music"] = new() { ".flac", ".ogg", ".mp3", ".wav" },
    };

    public override bool IsDownloadingMusicNeeded(string sourceDirectory)
        => true;

    public override bool IsDownloadingExpansionNeeded(string sourceDirectory)
        => true;

    public override async Task CopyOriginalGameFiles(
        string sourceDirectory,
        string targetDirectory,
        IProgress<InstallProgress> progress,
        bool importSaves
    )
    {
        await InstallUtils.CopyDirectoryTree(
            sourceDirectory,
            targetDirectory,
            progress,
            file => IsMatch(sourceDirectory, file, importSaves),
            path => ConvertTargetPath(path)
        );

        string musicDir = Path.Combine(targetDirectory, "music");
        string audioDir = Path.Combine(sourceDirectory, "audio");
        if ((Directory.Exists(musicDir) && Directory.EnumerateFiles(musicDir).Any()) || !Directory.Exists(audioDir))
        {
            return;
        }

        await InstallUtils.CopyDirectoryTree(
            Path.Combine(sourceDirectory, "audio"),
            Path.Combine(targetDirectory, "audio"),
            progress,
            null,
            path => ConvertTargetPath(path)
        );
    }

    private static bool IsMatch(string sourceDirectory, string path, bool importSaves)
    {
        string[] parts = Path.GetRelativePath(sourceDirectory, path).ToLower().Split('\\');
        if (parts.Length == 1 && importSaves && Regex.IsMatch(parts[0], @"savegame.\d+", RegexOptions.IgnoreCase))
        {
            return true;
        }

        return parts.Length > 0
            && _targetFiles.ContainsKey(parts[0])
            && (_targetFiles[parts[0]].Contains(".*") || _targetFiles[parts[0]].Contains(Path.GetExtension(path).ToLower()));
    }
}
