using System.IO;
using System.Text;

using System.Text.RegularExpressions;
using TRX_InstallerLib.Models;

namespace TRX_InstallerLib.Utils;

public static class ShortcutUtils
{
    public static void CreateShortcut(string shortcutPath, string targetPath, string name, string[]? args = null)
    {
        var fileInfo = File.Exists(targetPath) ? new FileInfo(targetPath) : null;
        using var stream = File.Open(Path.ChangeExtension(shortcutPath, "lnk"), FileMode.Create);
        using var bw = new BinaryWriter(stream);

        void writeShellLinkHeader()
        {
            // HeaderSize
            bw.Write(0x4C);

            // LinkCLSID
            bw.Write(new byte[] { 0x01, 0x14, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 });

            // LinkFlags
            bw.Write(
                (1 << 0) // HasLinkTargetIDList
                | (1 << 2) // HasName
                | (1 << 3) // HasRelativePath
                | (1 << 4) // HasWorkingDir
                | (1 << 5) // HasArguments
                | (1 << 7) // IsUnicode
                | (1 << 8) // ForceNoLinkInfo
            );

            if (fileInfo is not null)
            {
                bw.Write((int)fileInfo.Attributes); // FileAttributes
                bw.Write(fileInfo.CreationTimeUtc.ToFileTime()); // CreationTime
                bw.Write(fileInfo.LastAccessTimeUtc.ToFileTime()); // AccessTime
                bw.Write(fileInfo.LastWriteTimeUtc.ToFileTime()); // WriteTime
                bw.Write((int)fileInfo.Length); // FileSize
            }
            else
            {
                bw.Write(0); // FileAttributes
                bw.Write((long)0); // CreationTime
                bw.Write((long)0); // AccessTime
                bw.Write((long)0); // WriteTime
                bw.Write(0); // FileSize
            }
            bw.Write(0); // IconIndex
            bw.Write(1); // ShowCommand - SW_SHOWNORMAL
            bw.Write((short)0); // HotKey
            bw.Write((short)0); // Reserved1
            bw.Write(0); // Reserved2
            bw.Write(0); // Reserved3
        }

        void writeLinkTargetIDList()
        {
            var idListSizePos = (int)bw.BaseStream.Position;
            bw.Write((ushort)0); // IDListSize

            // CLSID for this computer
            bw.Write((short)(0x12 + 2));
            bw.Write(new byte[] { 0x1F, 0x50, 0xE0, 0x4F, 0xD0, 0x20, 0xEA, 0x3A, 0x69, 0x10, 0xA2, 0xD8, 0x08, 0x00, 0x2B, 0x30, 0x30, 0x9D });

            // Root directory
            var rootPrefix = "/";
            var root = Path.GetPathRoot(targetPath)!;
            var rootIdData = Encoding.Default.GetBytes(rootPrefix + root)
                .Concat(Enumerable.Repeat((byte)0, 21).ToArray())
                .Concat(new byte[] { 0x00 }).ToArray();
            bw.Write((short)(rootIdData.Length + 2));
            bw.Write(rootIdData);

            var targetLeafPrefix = fileInfo is not null && (fileInfo.Attributes & FileAttributes.Directory) != 0
                ? new byte[] { 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
                : new byte[] { 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
            var targetLeaf = Path.GetRelativePath(root, targetPath);
            var targetLeafIdData = targetLeafPrefix.Concat(Encoding.Default.GetBytes(targetLeaf)).Concat(new byte[] { 0x00 }).ToArray();
            bw.Write((short)(targetLeafIdData.Length + 2));
            bw.Write(targetLeafIdData);

            var idListSize = (int)bw.BaseStream.Position - idListSizePos;

            bw.Write((short)0);

            // fix offsets
            // IDListSize
            bw.Seek(idListSizePos, SeekOrigin.Begin);
            bw.Write((short)idListSize);

            // restore pos
            bw.Seek(idListSizePos + idListSize + 2, SeekOrigin.Begin);
        }

        void writeStringData()
        {
            // NAME
            bw.Write((short)name.Length);
            bw.Write(Encoding.Unicode.GetBytes(name));

            // RELATIVE_PATH
            var relativePath = Path.GetFileName(targetPath);
            bw.Write((short)relativePath.Length);
            bw.Write(Encoding.Unicode.GetBytes(relativePath));

            // WORKING_DIR
            var targetDir = Path.GetDirectoryName(targetPath)!;
            bw.Write((short)targetDir.Length);
            bw.Write(Encoding.Unicode.GetBytes(targetDir));

            // ARGUMENTS
            var cmdline = args is null ? "" : string.Join(
                " ",
                args.Select(
                    arg =>
                    {
                        if (string.IsNullOrEmpty(arg))
                            return arg;
                        string value = Regex.Replace(arg, @"(\\*)" + "\"", @"$1\$0");
                        value = Regex.Replace(value, @"^(.*\s.*?)(\\*)$", "\"$1$2$2\"");
                        return value;
                    }
                ).ToArray()
            );
            bw.Write((short)cmdline.Length);
            bw.Write(Encoding.Unicode.GetBytes(cmdline));
        }

        writeShellLinkHeader();
        writeLinkTargetIDList();
        writeStringData();
    }

    /// <summary>
    /// .NET Core compatible .lnk reader.
    /// MS Documentation:
    /// https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-shllink/16cb4ca1-9339-4d0c-a68d-bf1d6cc0f943?redirectedfrom=MSDN
    /// </summary>
    public static string? GetLnkTargetPath(string filepath)
    {
        using var br = new BinaryReader(File.OpenRead(filepath));

        var headerSize = br.ReadUInt32();
        if (headerSize != 0x4C)
        {
            throw new ApplicationException(Language.Instance.Controls!["shortcut_signature_failure"]);
        }

        br.ReadBytes(0x10); // skip LinkCLSID

        // LinkFlags
        var linkFlags = br.ReadUInt32();

        br.ReadBytes(4); // skip FileAttributes
        br.ReadBytes(8); // skip CreationTime
        br.ReadBytes(8); // skip AccessTime
        br.ReadBytes(8); // skip WriteTime
        br.ReadBytes(4); // skip FileSize
        br.ReadBytes(4); // skip IconIndex
        br.ReadBytes(4); // skip ShowCommand
        br.ReadBytes(2); // skip Hotkey
        br.ReadBytes(2); // skip Reserved
        br.ReadBytes(4); // skip Reserved2
        br.ReadBytes(4); // skip Reserved3

        var hasLinkTargetIDList = (linkFlags & (1 << 0)) != 0;
        var hasLinkInfo = (linkFlags & (1 << 1)) != 0;
        var hasName = (linkFlags & (1 << 2)) != 0;
        var hasRelativePath = (linkFlags & (1 << 3)) != 0;
        var hasWorkingDir = (linkFlags & (1 << 4)) != 0;
        var isUnicode = (linkFlags & (1 << 7)) != 0;

        // if the HasLinkTargetIDList bit, skip LinkTargetIDList
        if (hasLinkTargetIDList)
        {
            var skip = br.ReadUInt16();
            br.ReadBytes(skip);
        }

        if (hasLinkInfo)
        {
            // get the number of bytes the path contains
            var linkInfoSize = br.ReadUInt32();
            br.ReadBytes(4); // skip LinkInfoHeaderSize
            br.ReadBytes(4); // skip LinkInfoFlags
            br.ReadBytes(4); // skip VolumeIDOffset

            // Find the location of the LocalBasePath position
            var localPathBaseOffset = br.ReadUInt32();
            // Skip to the path position
            // (subtract the length of the read (4 bytes), the length of the skip (12 bytes), and
            // the length of the localPathBaseOffset read (4 bytes) from the localPathBaseOffset)
            br.ReadBytes((int)localPathBaseOffset - 0x14);
            var size = linkInfoSize - localPathBaseOffset - 0x02;
            var bytePath = br.ReadBytes((int)size);
            var path = Encoding.UTF8.GetString(bytePath, 0, bytePath.Length);
            return path;
        }

        if (hasName)
        {
            var _ = isUnicode ? br.ReadSystemCodepageString() : br.ReadUtf16String(); // skip Name
        }

        string? relativePath = null;
        if (hasRelativePath)
        {
            relativePath = isUnicode ? br.ReadSystemCodepageString() : br.ReadUtf16String();
        }

        string? workingDir = null;
        if (hasWorkingDir)
        {
            workingDir = isUnicode ? br.ReadSystemCodepageString() : br.ReadUtf16String();
        }

        if (workingDir is not null && relativePath is not null)
        {
            return Path.Combine(workingDir, relativePath);
        }
        if (workingDir is not null)
        {
            return workingDir;
        }
        if (relativePath is not null)
        {
            return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), relativePath);
        }

        throw new ApplicationException(Language.Instance.Controls!["shortcut_target_failure"]);
    }
}
