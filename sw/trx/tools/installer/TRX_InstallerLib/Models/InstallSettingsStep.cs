using System.Windows.Input;
using TRX_InstallerLib.Utils;

namespace TRX_InstallerLib.Models;

public class InstallSettingsStep : BaseLanguageViewModel, IStep
{
    public InstallSettingsStep(InstallSettings installSettings)
    {
        InstallSettings = installSettings;
        InstallSettings.PropertyChanged += (sender, e) =>
        {
            NotifyPropertyChanged(nameof(CanProceedToNextStep));
        };
    }

    public bool CanProceedToNextStep => InstallSettings.TargetDirectory != null;
    public bool CanProceedToPreviousStep => true;
    public InstallSettings InstallSettings { get; }
    public string SidebarImage => AssemblyUtils.GetEmbeddedResourcePath("side2.jpg");


    private RelayCommand? _chooseLocationCommand;
    public ICommand ChooseLocationCommand
    {
        get => _chooseLocationCommand ??= new RelayCommand(ChooseLocation);
    }

    private void ChooseLocation()
    {
        var result = FileBrowser.Browse(InstallSettings.TargetDirectory);
        if (result is not null)
        {
            InstallSettings.TargetDirectory = result;
        }
    }
}
