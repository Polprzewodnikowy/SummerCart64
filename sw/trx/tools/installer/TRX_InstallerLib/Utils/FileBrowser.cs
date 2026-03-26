using TRX_InstallerLib.Models;

namespace TRX_InstallerLib.Utils;

public class FileBrowser
{
    public static string? Browse(string? initialDirectory)
    {
        using var dlg = new FolderBrowserDialog()
        {
            Description = Language.Instance.Controls!["label_select_folder"],
            SelectedPath = initialDirectory,
            ShowNewFolderButton = true,
        };
        if (dlg.ShowDialog() == DialogResult.OK)
        {
            return dlg.SelectedPath;
        }
        return null;
    }
}
