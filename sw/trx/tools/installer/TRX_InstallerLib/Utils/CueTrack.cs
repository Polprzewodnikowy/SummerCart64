using System.IO;
using TRX_InstallerLib.Models;

namespace TRX_InstallerLib.Utils;

public class CueTrack
{
    public const int SectorLength = 2352;
    public bool Audio;
    public long StopSector;
    public bool SwapAudioByteOrder;
    public bool TruncatePsx;
    public bool WavFormat;

    public CueTrack(string binFilePath, int trackNumber, string mode, string time)
    {
        BinFilePath = binFilePath;
        TrackNumber = trackNumber;
        SetMode(mode);
        StartSector = ToFrames(time);
    }

    public enum TrackExtension
    {
        ISO, CDR, WAV, UGH
    }

    public string BinFilePath { get; private set; }
    public int BlockSize { get; private set; }
    public int BlockStart { get; private set; }
    public TrackExtension FileExtension { get; private set; }

    public long StartPosition
    {
        get { return StartSector * SectorLength; }
    }

    public long StartSector { get; private set; }

    public long Stop { get; set; }

    public long TotalBytes
    {
        get { return (StopSector - StartSector + 1) * BlockSize; }
    }

    public int TrackNumber { get; private set; }

    public void Write(string targetPath, IProgress<InstallProgress> progress)
    {
        using FileStream fileStream = OpenBinFile();
        try
        {
            using Stream stream = File.OpenWrite(targetPath);
            if (Audio && WavFormat)
            {
                byte[] header = MakeWavHeader(TotalBytes);
                stream.Write(header, 0, header.Length);
            }
            long currentPosition = StartPosition;
            long sector = StartSector;
            long convertedBytes = 0;

            byte[] buf = new byte[SectorLength];
            while (sector <= StopSector && fileStream.Read(buf, 0, SectorLength) > 0)
            {
                if (Audio && SwapAudioByteOrder)
                {
                    DoByteSwap(buf);
                }

                stream.Write(buf, BlockStart, BlockSize);
                currentPosition += SectorLength;
                convertedBytes += BlockSize;

                if (currentPosition / SectorLength % 500 == 0)
                {
                    progress.Report(new InstallProgress
                    {
                        MaximumValue = (int)TotalBytes,
                        CurrentValue = (int)convertedBytes,
                        Description = Language.Instance.Controls!["progress_converting_bin"],
                    });
                }

                sector++;
            }
        }
        catch (Exception e)
        {
            throw new ApplicationException(string.Format(Language.Instance.Controls!["progress_track_write_failure"], targetPath, e.Message));
        }

        progress.Report(new InstallProgress
        {
            MaximumValue = (int)TotalBytes,
            CurrentValue = (int)TotalBytes,
            Description = Language.Instance.Controls!["progress_converting_bin"],
        });
    }

    private static byte[] MakeWavHeader(long length)
    {
        const int WAV_RIFF_HLEN = 12;
        const int WAV_FORMAT_HLEN = 24;
        const int WAV_DATA_HLEN = 8;
        const int WAV_HEADER_LEN = WAV_RIFF_HLEN + WAV_FORMAT_HLEN + WAV_DATA_HLEN;

        MemoryStream memoryStream = new(WAV_HEADER_LEN);
        using (BinaryWriter writer = new(memoryStream))
        {
            // RIFF header
            writer.Write("RIFF".ToCharArray());
            uint dwordValue = (uint)length + WAV_DATA_HLEN + WAV_FORMAT_HLEN + 4;
            writer.Write(dwordValue);  // length of file, starting from WAVE
            writer.Write("WAVE".ToCharArray());
            // FORMAT header
            writer.Write("fmt ".ToCharArray());
            dwordValue = 0x10;     // length of FORMAT header
            writer.Write(dwordValue);
            ushort wordValue = 0x01;     // constant
            writer.Write(wordValue);
            wordValue = 0x02;   // channels
            writer.Write(wordValue);
            dwordValue = 44100; // sample rate
            writer.Write(dwordValue);
            dwordValue = 44100 * 4; // bytes per second
            writer.Write(dwordValue);
            wordValue = 4;      // bytes per sample
            writer.Write(wordValue);
            wordValue = 2 * 8;  // bits per channel
            writer.Write(wordValue);
            // DATA header
            writer.Write("data".ToCharArray());
            dwordValue = (uint)length;
            writer.Write(dwordValue);
        }
        return memoryStream.ToArray();
    }

    private static long ToFrames(string time)
    {
        string[] segs = time.Split(':');

        int mins = int.Parse(segs[0]);
        int secs = int.Parse(segs[1]);
        int frames = int.Parse(segs[2]);

        return (mins * 60 + secs) * 75 + frames;
    }

    private void DoByteSwap(byte[] buf)
    {
        // swap low and high bytes
        int p = BlockStart;
        int ep = BlockSize;
        while (p < ep)
        {
            (buf[p + 1], buf[p]) = (buf[p], buf[p + 1]);
            p += 2;
        }
    }

    private FileStream OpenBinFile()
    {
        FileStream fileStream;
        try
        {
            fileStream = File.OpenRead(BinFilePath);
        }
        catch (Exception e)
        {
            throw new ApplicationException(string.Format(Language.Instance.Controls!["progress_bin_failure"], BinFilePath, e.Message));
        }
        try
        {
            fileStream.Seek(StartPosition, SeekOrigin.Begin);
        }
        catch (Exception e)
        {
            throw new ApplicationException(string.Format(Language.Instance.Controls!["progress_track_seek_failure"], e.Message));
        }
        return fileStream;
    }

    private void SetMode(string mode)
    {
        Audio = false;
        BlockStart = 0;
        FileExtension = TrackExtension.ISO;

        switch (mode.ToUpper())
        {
            case "AUDIO":
                BlockSize = 2352;
                Audio = true;
                FileExtension = WavFormat ? TrackExtension.WAV : TrackExtension.CDR;
                break;

            case "MODE1/2352":
                BlockStart = 16;
                BlockSize = 2048;
                break;

            case "MODE2/2336":
                // WAS 2352 in V1.361B still work? What if MODE2/2336 single track bin, still 2352 sectors?
                BlockStart = 16;
                BlockSize = 2336;
                break;

            case "MODE2/2352":
                if (TruncatePsx)
                {
                    // PSX: truncate from 2352 to 2336 byte tracks
                    BlockSize = 2336;
                }
                else
                {
                    // Normal MODE2/2352
                    BlockStart = 24;
                    BlockSize = 2048;
                }
                break;

            default:
                BlockSize = 2352;
                FileExtension = TrackExtension.UGH;
                break;
        }
    }
}
