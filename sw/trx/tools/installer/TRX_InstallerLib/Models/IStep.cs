using System.ComponentModel;

namespace TRX_InstallerLib.Models;

public interface IStep : INotifyPropertyChanged
{
    bool CanProceedToNextStep { get; }
    bool CanProceedToPreviousStep { get; }
    string SidebarImage { get; }
}
