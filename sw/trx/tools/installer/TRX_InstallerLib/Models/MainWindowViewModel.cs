using System.IO;
using System.Windows;
using System.Windows.Input;
using TRX_InstallerLib.Installers;
using TRX_InstallerLib.Utils;

namespace TRX_InstallerLib.Models;

public class MainWindowViewModel : BaseLanguageViewModel
{
    public MainWindowViewModel(IEnumerable<IInstallSource> installSources)
    {
        _sourceStep = new SourceStep(installSources);
        _currentStep = _sourceStep;
        _installSettings = new InstallSettings();
    }

    public ICommand CloseWindowCommand
    {
        get => _closeWindowCommand ??= new RelayCommand<Window>(CloseWindow);
    }

    public IStep CurrentStep
    {
        get => _currentStep;
        set
        {
            _currentStep = value;
            _goToPreviousStepCommand?.RaiseCanExecuteChanged();
            _goToNextStepCommand?.RaiseCanExecuteChanged();
            _currentStep.PropertyChanged += (sender, e) =>
            {
                _goToPreviousStepCommand?.RaiseCanExecuteChanged();
                _goToNextStepCommand?.RaiseCanExecuteChanged();
            };
            NotifyPropertyChanged();
            NotifyPropertyChanged(nameof(IsFinalStep));
        }
    }

    public ICommand GoToNextStepCommand
    {
        get => _goToNextStepCommand ??= new RelayCommand(GoToNextStep, CanGoToNextStep);
    }

    public ICommand GoToPreviousStepCommand
    {
        get => _goToPreviousStepCommand ??= new RelayCommand(GoToPreviousStep, CanGoToPreviousStep);
    }

    public bool IsFinalStep
    {
        get => CurrentStep is FinishStep;
    }

    public bool IsSidebarVisible
    {
        get => WindowWidth >= 500;
    }

    public int WindowWidth
    {
        get => _windowWidth;
        set
        {
            if (value != _windowWidth)
            {
                _windowWidth = value;
                NotifyPropertyChanged(nameof(IsSidebarVisible));
            }
        }
    }

    private const bool _autoFinishInstallStep = false;

    private RelayCommand<Window>? _closeWindowCommand;

    private IStep _currentStep;
    private FinishSettings? _finishSettings;
    private RelayCommand? _goToNextStepCommand;
    private RelayCommand? _goToPreviousStepCommand;
    private readonly InstallSettings _installSettings;
    private readonly IStep _sourceStep;
    private int _windowWidth;

    private bool CanGoToNextStep()
    {
        return CurrentStep.CanProceedToNextStep;
    }

    private bool CanGoToPreviousStep()
    {
        return CurrentStep.CanProceedToPreviousStep;
    }

    private void CloseWindow(Window? window)
    {
        if (_finishSettings is not null && _finishSettings.LaunchGame)
        {
            if (_installSettings.TargetDirectory is null)
            {
                throw new NullReferenceException();
            }
            ProcessUtils.Start(Path.Combine(_installSettings.TargetDirectory, TRXConstants.Instance.Exe));
        }
        if (_finishSettings is not null && _finishSettings.OpenGameDirectory)
        {
            if (_installSettings.TargetDirectory is null)
            {
                throw new NullReferenceException();
            }
            ProcessUtils.Start(_installSettings.TargetDirectory);
        }
        window?.Close();
    }

    private void GoToNextStep()
    {
        if (CurrentStep is SourceStep sourceStep)
        {
            var installSource = sourceStep.SelectedInstallationSource!.InstallSource;
            _installSettings.InstallSource = installSource;
            _installSettings.SourceDirectory = sourceStep.SelectedInstallationSource.SourceDirectory;
            CurrentStep = new InstallSettingsStep(_installSettings);
        }
        else if (CurrentStep is InstallSettingsStep targetStep)
        {
            var installStep = new InstallStep(targetStep.InstallSettings);
            installStep.RunInstall();
            installStep.PropertyChanged += (sender, e) =>
            {
                if (_autoFinishInstallStep && installStep.CanProceedToNextStep)
                {
                    _finishSettings = new FinishSettings();
                    CurrentStep = new FinishStep(_finishSettings);
                }
            };
            CurrentStep = installStep;
        }
        else if (CurrentStep is InstallStep)
        {
            _finishSettings = new FinishSettings();
            CurrentStep = new FinishStep(_finishSettings);
        }
    }

    private void GoToPreviousStep()
    {
        CurrentStep = _sourceStep;
    }
}
