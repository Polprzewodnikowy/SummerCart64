namespace TRX_InstallerLib.Utils;

public class InstallProgress
{
    public int? CurrentValue { get; set; }
    public string? Description { get; set; }
    public bool Finished { get; set; }
    public int? MaximumValue { get; set; }
}
