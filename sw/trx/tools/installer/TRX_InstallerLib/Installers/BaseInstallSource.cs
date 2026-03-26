using System.IO;
using TRX_InstallerLib.Models;
using TRX_InstallerLib.Utils;

namespace TRX_InstallerLib.Installers;

public abstract class BaseInstallSource : IInstallSource
{
    public abstract IEnumerable<string> DirectoriesToTry { get; }

    public virtual string ImageSource
    {
        get => AssemblyUtils.GetEmbeddedResourcePath($"{SourceName}.png");
    }

    public abstract bool IsImportingSavesSupported { get; }
    public abstract string SourceName { get; }

    public virtual string SuggestedInstallationDirectory
    {
        get => Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),
            TRXConstants.Instance.Game!);
    }

    public abstract Task CopyOriginalGameFiles(
        string sourceDirectory,
        string targetDirectory,
        IProgress<InstallProgress> progress,
        bool importSaves
    );

    public abstract bool IsDownloadingMusicNeeded(string sourceDirectory);

    public abstract bool IsDownloadingExpansionNeeded(string sourceDirectory);

    public abstract bool IsGameFound(string sourceDirectory);

    public static string ConvertTargetPath(string relPath)
    {
        string ext = Path.GetExtension(relPath).ToLower();
        switch (ext)
        {
            case ".pcx":
                relPath = @$"data\images\og\{Path.GetFileName(relPath)}";
                break;
            case ".json5":
            case ".exe":
                return relPath;
            default:
                break;
        }

        return relPath.ToLower();
    }
}
