using Microsoft.Win32;
using System.IO;
using System.IO.Compression;
using System.Text.RegularExpressions;
using TRX_InstallerLib.Models;
using TRX_InstallerLib.Utils;

namespace TRX_InstallerLib.Installers;

public static class InstallUtils
{
    private static readonly string _registryStorageKey;

    static InstallUtils()
    {
        _registryStorageKey = $@"Software\{TRXConstants.Instance.Game}";
    }

    public static async Task CopyDirectoryTree(
        string sourceDirectory,
        string targetDirectory,
        IProgress<InstallProgress> progress,
        Func<string, bool>? filterCallback = null,
        Func<string, string>? targetCallback = null,
        Func<string, bool>? overwriteCallback = null
    )
    {
        try
        {
            progress.Report(new InstallProgress { Description = Language.Instance.Controls!["progress_scanning"] });
            var files = Directory.GetFiles(sourceDirectory, "*", SearchOption.AllDirectories);
            var currentProgress = 0;
            var maximumProgress = files.Length;
            foreach (var sourcePath in files)
            {
                if (filterCallback is not null && !filterCallback(sourcePath))
                {
                    continue;
                }
                var relPath = Path.GetRelativePath(sourceDirectory, sourcePath);
                if (targetCallback is not null)
                {
                    relPath = targetCallback(relPath) ?? relPath;
                }
                var targetPath = Path.Combine(targetDirectory, relPath);
                var isSamePath = string.Equals(Path.GetFullPath(sourcePath), Path.GetFullPath(targetPath), StringComparison.OrdinalIgnoreCase);
                if (!File.Exists(targetPath) || (overwriteCallback is not null && overwriteCallback(sourcePath) && !isSamePath))
                {
                    progress.Report(new InstallProgress
                    {
                        CurrentValue = currentProgress,
                        MaximumValue = maximumProgress,
                        Description = string.Format(Language.Instance.Controls!["progress_copying"], relPath),
                    });
                    Directory.CreateDirectory(Path.GetDirectoryName(targetPath)!);
                    await Task.Run(() => File.Copy(sourcePath, targetPath, true));

                    try
                    {
                        var file = new FileInfo(targetPath);
                        if (file.Attributes.HasFlag(FileAttributes.ReadOnly))
                        {
                            file.IsReadOnly = false;
                        }
                    }
                    catch { }
                }
                else
                {
                    progress.Report(new InstallProgress
                    {
                        CurrentValue = currentProgress,
                        MaximumValue = maximumProgress,
                        Description = string.Format(Language.Instance.Controls!["progress_skipped"], relPath),
                    });
                }

                currentProgress++;
            }
        }
        catch (Exception e)
        {
            throw new ApplicationException(e.Message);
        }
    }

    public static void CreateDesktopShortcut(string name, string title, string targetPath, string[]? args = null)
    {
        var shortcutPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), $"{name}.lnk");
        ShortcutUtils.CreateShortcut(shortcutPath, targetPath, title, args);
    }

    public static async Task<byte[]> DownloadFile(string url, IProgress<InstallProgress> progress)
    {
        HttpProgressClient wc = new();
        progress.Report(new InstallProgress { Description = string.Format(Language.Instance.Controls!["progress_init_download"], url) });
        wc.DownloadProgressChanged += (totalBytesToReceive, bytesReceived) =>
        {
            progress.Report(new InstallProgress
            {
                CurrentValue = (int)bytesReceived,
                MaximumValue = (int)totalBytesToReceive,
                Description = string.Format(Language.Instance.Controls!["progress_downloading"], url),
            });
        };
        return await wc.DownloadDataTaskAsync(new Uri(url));
    }

    public static async Task DownloadZip(
        string url,
        string targetDirectory,
        IProgress<InstallProgress> progress
    )
    {
        var response = await DownloadFile(url, progress);
        using var stream = new MemoryStream(response);
        await ExtractZip(stream, targetDirectory, progress);
    }

    public static async Task ExtractZip(
        Stream stream,
        string targetDirectory,
        IProgress<InstallProgress> progress,
        Func<string, bool>? filterCallback = null,
        bool overwrite = false
    )
    {
        try
        {
            using var zip = new ZipArchive(stream);
            progress.Report(new InstallProgress
            {
                Description = Language.Instance.Controls!["progress_scanning_zip"],
            });
            var currentProgress = 0;
            var maximumProgress = zip.Entries.Count;
            foreach (var entry in zip.Entries)
            {
                if (new Regex(@"[\\/]$").IsMatch(entry.FullName))
                {
                    continue;
                }
                if (filterCallback is not null && !filterCallback(entry.FullName))
                {
                    continue;
                }
                var relPath = BaseInstallSource.ConvertTargetPath(new Regex(@"[\\/]").Replace(entry.FullName, Path.DirectorySeparatorChar.ToString()));
                var targetPath = Path.Combine(targetDirectory, relPath);

                if (!File.Exists(targetPath) || overwrite)
                {
                    progress.Report(new InstallProgress
                    {
                        CurrentValue = currentProgress,
                        MaximumValue = maximumProgress,
                        Description = string.Format(Language.Instance.Controls!["progress_extracting"], relPath),
                    });

                    Directory.CreateDirectory(Path.GetDirectoryName(targetPath)!);
                    await Task.Run(() => entry.ExtractToFile(targetPath, true));
                }
                else
                {
                    progress.Report(new InstallProgress
                    {
                        CurrentValue = currentProgress,
                        MaximumValue = maximumProgress,
                        Description = string.Format(Language.Instance.Controls!["progress_extracting_skipped"], relPath),
                    });
                }

                currentProgress++;
            }
        }
        catch (Exception e)
        {
            throw new ApplicationException(e.Message);
        }
    }

    public static IEnumerable<string> GetDesktopShortcutDirectories()
    {
        foreach (
            var shortcutPath in Directory.EnumerateFiles(
                Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), "*.lnk"
            )
        )
        {
            string? lnkPath;
            try
            {
                lnkPath = ShortcutUtils.GetLnkTargetPath(shortcutPath);
            }
            catch (Exception)
            {
                continue;
            }
            if (lnkPath is not null)
            {
                var dirName = Path.GetDirectoryName(lnkPath);
                if (dirName is not null)
                {
                    yield return dirName;
                }
            }
        }
    }

    public static void StoreInstallationPath(string installPath)
    {
        using var key = Registry.CurrentUser.CreateSubKey(_registryStorageKey);
        key?.SetValue("InstallPath", installPath);
    }

    public static string? GetPreviousInstallationPath()
    {
        using var key = Registry.CurrentUser.OpenSubKey(_registryStorageKey);
        return key?.GetValue("InstallPath")?.ToString();
    }
}
