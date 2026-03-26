using TRX_InstallerLib.Installers;
using TRX_InstallerLib.Utils;

namespace TRX_InstallerLib.Models;

public class InstallStep : BaseLanguageViewModel, IStep
{
    public InstallStep(InstallSettings installSettings)
    {
        Logger = new Logger();
        InstallSettings = installSettings;
    }

    public bool CanProceedToNextStep
    {
        get => _canProceedToNextStep;
        set
        {
            if (value != _canProceedToNextStep)
            {
                _canProceedToNextStep = value;
                NotifyPropertyChanged();
            }
        }
    }

    public bool CanProceedToPreviousStep => false;

    public int CurrentProgress
    {
        get { return _currentProgress; }
        set
        {
            if (value != _currentProgress)
            {
                _currentProgress = value;
                NotifyPropertyChanged();
            }
        }
    }

    public string? Description
    {
        get => _description;
        set
        {
            if (value != _description)
            {
                _description = value;
                NotifyPropertyChanged();
            }
        }
    }

    public InstallSettings InstallSettings { get; }
    public Logger Logger { get; }

    public int MaximumProgress
    {
        get { return _maximumProgress; }
        set
        {
            if (value != _maximumProgress)
            {
                _maximumProgress = value;
                NotifyPropertyChanged();
                NotifyPropertyChanged(nameof(CanProceedToNextStep));
            }
        }
    }

    public string SidebarImage => AssemblyUtils.GetEmbeddedResourcePath("side3.jpg");

    public void RunInstall()
    {
        var progress = new Progress<InstallProgress>();
        progress.ProgressChanged += (sender, progress) =>
        {
            if (progress.CurrentValue is not null && progress.MaximumValue is not null)
            {
                CurrentProgress = progress.CurrentValue.Value;
                MaximumProgress = progress.MaximumValue.Value;
            }
            else
            {
                CurrentProgress = progress.Finished ? 1 : 0;
                MaximumProgress = 1;
            }
            Description = progress.Description;
            if (progress.Description is not null)
            {
                Logger.RaiseLogEvent(progress.Description);
            }
            if (progress.Finished)
            {
                CanProceedToNextStep = true;
            }
        };

        Task.Run(async () =>
        {
            try
            {
                var executor = new InstallExecutor(InstallSettings);
                await executor.ExecuteInstall(progress);
            }
            catch (Exception ex)
            {
                Logger.RaiseLogEvent(ex.ToString());
            }
        });
    }

    private bool _canProceedToNextStep;
    private int _currentProgress = 0;
    private string? _description;
    private int _maximumProgress = 1;
}
