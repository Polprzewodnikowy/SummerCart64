using TRX_InstallerLib.Utils;

namespace TRX_InstallerLib.Models;

public class TRXConstants
{
    private static readonly string _constConfigPath = "Resources.const.json";

    public static TRXConstants Instance { get; private set; }

    static TRXConstants()
    {
        Instance = JsonUtils.LoadEmbeddedResource(_constConfigPath)?.ToObject<TRXConstants>() ?? new();
    }

    public string? Game { get; set; }
    public string? GoldGame { get; set; }
    public string? GoldFileIdentifier { get; set; }
    public string Exe => $"TRX.exe";
    public bool? AllowExpansionTypeSelection { get; set; }
    public string? GoldArgs { get; set; }
    public string? ShortcutTitle { get; set; }
    public Dictionary<ExpansionPackType, string>? GoldZips { get; set; }
}
