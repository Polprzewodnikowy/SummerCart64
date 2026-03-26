using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System.Globalization;
using TRX_InstallerLib.Utils;

namespace TRX_InstallerLib.Models;

public class Language
{
    private static readonly string _langPathFormat = "Resources.Lang.{0}.json";
    private static readonly string _defaultCulture = "en-US";

    public static Language Instance { get; private set; }

    public Dictionary<string, string>? Controls { get; set; }

    static Language()
    {
        CultureInfo defaultCulture = CultureInfo.GetCultureInfo(_defaultCulture);
        JObject defaultData = ReadLanguage(defaultCulture.TwoLetterISOLanguageName);

        if (CultureInfo.CurrentCulture != defaultCulture)
        {
            // Merge the main language first if it exists, and then the country specific if that exists.
            // e.g. fr.json would load first, then fr-BE.json.
            MergeLanguage(defaultData, CultureInfo.CurrentCulture.TwoLetterISOLanguageName);
            MergeLanguage(defaultData, CultureInfo.CurrentCulture.Name);
        }

        Instance = JsonConvert.DeserializeObject<Language>(defaultData.ToString())!;
    }

    private static JObject ReadLanguage(string tag)
    {
        return JsonUtils.LoadEmbeddedResource(string.Format(_langPathFormat, tag)) ?? new();
    }

    private static void MergeLanguage(JObject data, string tag)
    {
        JObject cultureData = ReadLanguage(tag);
        if (cultureData != null)
        {
            data.Merge(cultureData);
        }
    }
}
