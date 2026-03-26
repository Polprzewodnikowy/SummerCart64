using TRX_InstallerLib.Utils;

namespace TRX_InstallerLib.Models;

public class FinishStep : BaseLanguageViewModel, IStep
{
    public FinishStep(FinishSettings finishSettings)
    {
        FinishSettings = finishSettings;
    }

    public bool CanProceedToNextStep => false;
    public bool CanProceedToPreviousStep => false;
    public FinishSettings FinishSettings { get; }
    public string SidebarImage => AssemblyUtils.GetEmbeddedResourcePath("side4.jpg");
}
