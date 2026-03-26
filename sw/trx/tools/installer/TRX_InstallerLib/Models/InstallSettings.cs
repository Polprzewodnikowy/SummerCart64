using TRX_InstallerLib.Installers;
using TRX_InstallerLib.Utils;

namespace TRX_InstallerLib.Models;

public class InstallSettings : BaseNotifyPropertyChanged
{
    public bool CreateDesktopShortcut
    {
        get => _createDesktopShortcut;
        set
        {
            if (value != _createDesktopShortcut)
            {
                _createDesktopShortcut = value;
                NotifyPropertyChanged();
            }
        }
    }

    public bool DownloadMusic
    {
        get => _downloadMusic;
        set
        {
            if (value != _downloadMusic)
            {
                _downloadMusic = value;
                NotifyPropertyChanged();
            }
        }
    }

    public bool DownloadExpansionPack
    {
        get => _downloadExpansionPack;
        set
        {
            if (value != _downloadExpansionPack)
            {
                _downloadExpansionPack = value;
                NotifyPropertyChanged();
            }
        }
    }

    public bool AllowExpansionTypeSelection
    {
        get => _allowExpansionPackSelection;
        private set
        {
            if (value != _allowExpansionPackSelection)
            {
                _allowExpansionPackSelection = value;
                NotifyPropertyChanged();
            }
        }
    }

    public ExpansionPackType ExpansionPackType
    {
        get => _expansionPackType;
        set
        {
            if (value != _expansionPackType)
            {
                _expansionPackType = value;
                NotifyPropertyChanged();
            }
        }
    }

    public bool ImportSaves
    {
        get => _importSaves;
        set
        {
            if (value != _importSaves)
            {
                _importSaves = value;
                NotifyPropertyChanged();
            }
        }
    }

    public IInstallSource? InstallSource
    {
        get => _installSource;
        set
        {
            if (value != _installSource)
            {
                _installSource = value;
                DownloadMusic = SourceDirectory is not null && (_installSource?.IsDownloadingMusicNeeded(SourceDirectory) ?? false);
                AllowExpansionTypeSelection = TRXConstants.Instance.AllowExpansionTypeSelection ?? false;
                DownloadExpansionPack = SourceDirectory is not null && (_installSource?.IsDownloadingExpansionNeeded(SourceDirectory) ?? false);
                ImportSaves = _installSource?.IsImportingSavesSupported ?? false;
                TargetDirectory = _installSource?.SuggestedInstallationDirectory;
                NotifyPropertyChanged();
            }
        }
    }

    public bool IsDownloadingMusicNeeded
    {
        get
        {
            return SourceDirectory is not null && (InstallSource?.IsDownloadingMusicNeeded(SourceDirectory) ?? false);
        }
    }

    public bool IsDownloadingExpansionNeeded
    {
        get
        {
            return SourceDirectory is not null && (InstallSource?.IsDownloadingExpansionNeeded(SourceDirectory) ?? false);
        }
    }

    public string? SourceDirectory
    {
        get => _sourceDirectory;
        set
        {
            if (value != _sourceDirectory)
            {
                _sourceDirectory = value;
                NotifyPropertyChanged();
            }
        }
    }

    public string? TargetDirectory
    {
        get => _targetDirectory;
        set
        {
            if (value != _targetDirectory)
            {
                _targetDirectory = value;
                NotifyPropertyChanged();
            }
        }
    }

    private bool _createDesktopShortcut = true;
    private bool _downloadMusic;
    private bool _downloadExpansionPack;
    private bool _allowExpansionPackSelection;
    private ExpansionPackType _expansionPackType;
    private bool _importSaves;
    private IInstallSource? _installSource;
    private string? _sourceDirectory;
    private string? _targetDirectory;
}
