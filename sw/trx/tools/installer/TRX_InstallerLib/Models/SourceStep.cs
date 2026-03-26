using System.Collections.ObjectModel;
using TRX_InstallerLib.Installers;
using TRX_InstallerLib.Utils;

namespace TRX_InstallerLib.Models;

public class SourceStep : BaseLanguageViewModel, IStep
{
    public SourceStep(IEnumerable<IInstallSource> installSources)
    {
        // NOTE: the order also decides which installation source will be selected by default
        InstallationSources = new ObservableCollection<InstallSourceViewModel>
            (installSources.Select(i => new InstallSourceViewModel(i)));

        foreach (var installationSource in InstallationSources)
        {
            installationSource.PropertyChanged += (sender, e) =>
            {
                NotifyPropertyChanged(nameof(InstallationSources));
                if (installationSource == selectedInstallationSource)
                {
                    NotifyPropertyChanged(nameof(SelectedInstallationSource));
                }
            };
        }

        foreach (var source in InstallationSources)
        {
            if (source.IsAvailable)
            {
                SelectedInstallationSource = source;
            }
        }
    }

    public bool CanProceedToNextStep
    {
        get => SelectedInstallationSource != null && SelectedInstallationSource.IsAvailable;
    }

    public bool CanProceedToPreviousStep => false;
    public IEnumerable<InstallSourceViewModel> InstallationSources { get; private set; }

    public InstallSourceViewModel? SelectedInstallationSource
    {
        get => selectedInstallationSource;
        set
        {
            if (value != selectedInstallationSource)
            {
                selectedInstallationSource = value;
                NotifyPropertyChanged();
                NotifyPropertyChanged(nameof(CanProceedToNextStep));
            }
        }
    }

    public string SidebarImage => AssemblyUtils.GetEmbeddedResourcePath("side1.jpg");
    private InstallSourceViewModel? selectedInstallationSource;
}
