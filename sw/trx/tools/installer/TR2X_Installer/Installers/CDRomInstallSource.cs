using System.IO;

namespace TR2X_Installer.Installers;

public class CDRomInstallSource : GenericInstallSource
{
    public override IEnumerable<string> DirectoriesToTry
    {
        get
        {
            DriveInfo[] allDrives = DriveInfo.GetDrives();
            foreach (var drive in allDrives)
            {
                if (drive.DriveType == DriveType.CDRom && drive.IsReady)
                {
                    yield return drive.RootDirectory.FullName;
                }
            }
        }
    }

    public override bool IsImportingSavesSupported => false;
    public override string SourceName => "CDRom";

    public override bool IsGameFound(string sourceDirectory)
    {
        return File.Exists(Path.Combine(sourceDirectory, "fmv", "ancient.rpl"))
            && File.Exists(Path.Combine(sourceDirectory, "data", "wall.tr2"))
            && File.Exists(Path.Combine(sourceDirectory, "data", "main.sfx"));
    }
}
