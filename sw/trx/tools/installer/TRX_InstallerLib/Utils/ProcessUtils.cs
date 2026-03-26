using System.Diagnostics;
using System.IO;

namespace TRX_InstallerLib.Utils;

public static class ProcessUtils
{
    public static void Start(string fileName, string? arguments = null)
    {
        Process.Start(new ProcessStartInfo
        {
            FileName = fileName,
            Arguments = arguments,
            UseShellExecute = true,
            WorkingDirectory = new Uri(fileName).IsFile
                ? Path.GetDirectoryName(fileName)
                : null
        });
    }
}
