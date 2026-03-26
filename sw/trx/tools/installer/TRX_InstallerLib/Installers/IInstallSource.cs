using TRX_InstallerLib.Utils;

namespace TRX_InstallerLib.Installers;

public interface IInstallSource
{
    public IEnumerable<string> DirectoriesToTry { get; }

    public string ImageSource { get; }

    public string SourceName { get; }

    public string SuggestedInstallationDirectory { get; }

    public Task CopyOriginalGameFiles(
        string sourceDirectory,
        string targetDirectory,
        IProgress<InstallProgress> progress,
        bool importSaves
    );

    bool IsDownloadingMusicNeeded(string sourceDirectory);

    bool IsDownloadingExpansionNeeded(string sourceDirectory);

    public bool IsGameFound(string sourceDirectory);

    bool IsImportingSavesSupported { get; }
}
