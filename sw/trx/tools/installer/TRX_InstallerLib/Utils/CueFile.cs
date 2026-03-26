using System.IO;
using System.Text.RegularExpressions;
using TRX_InstallerLib.Models;

namespace TRX_InstallerLib.Utils;

public class CueFile
{
    public readonly List<CueTrack> TrackList = new();

    public CueFile(string cueFilePath)
    {
        _cueFilePath = cueFilePath;
        string cueFileContent;
        using (TextReader cueReader = new StreamReader(cueFilePath))
        {
            cueFileContent = cueReader.ReadToEnd();
        }

        MatchCollection fileMatches = _fileGroupRegex.Matches(cueFileContent);
        if (fileMatches.Count == 0)
        {
            throw new ApplicationException(string.Format(Language.Instance.Controls!["progress_cue_empty"], cueFilePath));
        }

        foreach (Match fileMatch in fileMatches.Cast<Match>())
        {
            var binFilePath = GetBinFilePath(fileMatch.Groups["name"].Value.Trim('"'));
            var matches = _trackRegex.Matches(fileMatch.Groups["content"].Value);

            if (matches.Count == 0)
            {
                throw new ApplicationException(string.Format(Language.Instance.Controls!["progress_cue_empty"], cueFilePath));
            }

            CueTrack? track = null;
            CueTrack? prevTrack = null;
            foreach (Match trackMatch in matches.Cast<Match>())
            {
                track = new CueTrack(
                    binFilePath,
                    int.Parse(trackMatch.Groups["track"].Value),
                    trackMatch.Groups["mode"].Value,
                    trackMatch.Groups["time"].Value);

                if (prevTrack != null)
                {
                    prevTrack.Stop = track.StartPosition - 1;
                    prevTrack.StopSector = track.StartSector;
                }
                TrackList.Add(track);
                prevTrack = track;
            }

            if (track == null)
            {
                return;
            }

            track.Stop = GetBinFileLength(binFilePath);
            track.StopSector = track.Stop / CueTrack.SectorLength;
        }
    }

    private static readonly Regex _fileGroupRegex = new(
        @"^file\s+(?<name>""[^""]+""|[^""\s]+)\s+(?<mode>\w+)\s+(?<content>(.(?!^file))*)",
        RegexOptions.IgnoreCase | RegexOptions.Multiline | RegexOptions.Singleline);

    private static readonly Regex _trackRegex = new(@"track\s+?(?<track>\d+?)\s+?(?<mode>\S+?)[\s$]+?index\s+?\d+?\s+?(?<time>\S*)",
             RegexOptions.IgnoreCase | RegexOptions.Multiline);

    private readonly string _cueFilePath;

    private static long GetBinFileLength(string binFilePath)
    {
        FileInfo fileInfo = new(binFilePath);
        return fileInfo.Length;
    }

    private string GetBinFilePath(string name)
    {
        var cueDirectory = Path.GetDirectoryName(_cueFilePath)!;
        string result = Path.Combine(cueDirectory, Path.GetFileName(name));
        if (!File.Exists(result))
        {
            result = Path.Combine(cueDirectory, Path.GetFileNameWithoutExtension(_cueFilePath) + ".bin");
        }
        return result;
    }
}
