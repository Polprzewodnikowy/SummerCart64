using DiscUtils.Iso9660;
using DiscUtils.Streams;
using Microsoft.Win32;
using System.IO;
using System.Text.RegularExpressions;
using TRX_InstallerLib.Installers;
using TRX_InstallerLib.Models;
using TRX_InstallerLib.Utils;

namespace TR1X_Installer.Installers;

public class GOGInstallSource : BaseInstallSource
{
    public override IEnumerable<string> DirectoriesToTry
    {
        get
        {
            yield return @"C:\Program Files (x86)\GOG Galaxy\Games\Tomb Raider 1";

            using var key = Registry.ClassesRoot.OpenSubKey(@"goggalaxy\shell\open\command");
            if (key is not null)
            {
                var value = key.GetValue("")?.ToString();
                if (value is not null && new Regex(@"""(?<path>[^""]+)""").Match(value) is { Success: true } match)
                {
                    yield return Path.Combine(Path.GetDirectoryName(match.Groups["path"].Value)!, @"Games\Tomb Raider 1");
                }
            }
        }
    }

    public override bool IsImportingSavesSupported => false;
    public override string SourceName => "GOG";

    public override Task CopyOriginalGameFiles(
        string sourceDirectory,
        string targetDirectory,
        IProgress<InstallProgress> progress,
        bool importSaves
    )
    {
        var cuePath = Path.Combine(sourceDirectory, "game.dat");
        var isoPath = Path.Combine(sourceDirectory, "game.iso");
        CueFile cueFile;
        try
        {
            cueFile = new CueFile(cuePath);
        }
        catch (Exception e)
        {
            throw new ApplicationException(string.Format(Language.Instance.Controls!["progress_cue_failure"], cuePath, e.Message));
        }

        try
        {
            var firstTrack = cueFile.TrackList.First();
            firstTrack.Write(isoPath, progress);
        }
        catch (Exception e)
        {
            throw new ApplicationException(string.Format(Language.Instance.Controls!["progress_converting_bin_failure"], e.Message));
        }

        try
        {
            using FileStream file = File.Open(isoPath, FileMode.Open, FileAccess.Read);
            using CDReader reader = new(file, true);
            int currentProgress = 0;

            progress.Report(new InstallProgress
            {
                MaximumValue = 1,
                CurrentValue = 0,
                Description = Language.Instance.Controls!["progress_scanning_source"],
            });
            var filesToExtract = GetFilesToExtract(reader.Root);
            progress.Report(new InstallProgress
            {
                MaximumValue = filesToExtract.Count(),
                CurrentValue = 0,
                Description = Language.Instance.Controls!["progress_preparing_extract"],
            });
            foreach (var path in filesToExtract)
            {
                var relPath = ConvertTargetPath(path);
                var targetPath = Path.Combine(targetDirectory, relPath);
                if (!File.Exists(targetPath))
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(targetPath)!);

                    using SparseStream sourceStream = reader.OpenFile(path, FileMode.Open, FileAccess.Read);
                    var readAllByte = new byte[sourceStream.Length];
                    sourceStream.Read(readAllByte, 0, readAllByte.Length);

                    using FileStream targetStream = new(targetPath, FileMode.Create);
                    targetStream.Position = 0;
                    targetStream.Write(readAllByte, 0, readAllByte.Length);
                }

                progress.Report(new InstallProgress
                {
                    MaximumValue = filesToExtract.Count(),
                    CurrentValue = ++currentProgress,
                    Description = string.Format(Language.Instance.Controls!["progress_extracting"], relPath)
                });
            }
        }
        catch (Exception e)
        {
            throw new ApplicationException(string.Format(Language.Instance.Controls!["progress_converting_iso_failure"], e.Message));
        }

        File.Delete(isoPath);

        return Task.CompletedTask;
    }

    public override bool IsDownloadingMusicNeeded(string sourceDirectory)
    {
        return true;
    }

    public override bool IsDownloadingExpansionNeeded(string sourceDirectory)
    {
        return true;
    }

    public override bool IsGameFound(string sourceDirectory)
    {
        return File.Exists(Path.Combine(sourceDirectory, "GAME.GOG"));
    }

    private static IEnumerable<string> GetFilesToExtract(DiscUtils.DiscDirectoryInfo root)
    {
        var regex = new Regex(@"^(data|fmv)[\\/].*$", RegexOptions.IgnoreCase);
        foreach (var dir in root.GetDirectories())
        {
            foreach (var filePath in GetFilesToExtract(dir))
            {
                yield return filePath;
            }
        }
        foreach (var file in root.GetFiles())
        {
            string filePath = file.FullName;
            if (regex.IsMatch(filePath))
            {
                yield return filePath;
            }
        }
    }
}
