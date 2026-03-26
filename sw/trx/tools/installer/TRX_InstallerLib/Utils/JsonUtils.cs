using Newtonsoft.Json.Linq;
using System.IO;

namespace TRX_InstallerLib.Utils;

public static class JsonUtils
{
    public static JObject? LoadEmbeddedResource(string path)
    {
        // Try to locate the data in this assembly first, then merge it
        // with the same in the entry assembly if relevant.
        JObject? data = null;

        if (AssemblyUtils.ResourceExists(path, true))
        {
            using Stream stream = AssemblyUtils.GetResourceStream(path, true);
            using StreamReader reader = new(stream);
            data = JObject.Parse(reader.ReadToEnd());
        }

        if (AssemblyUtils.ResourceExists(path, false))
        {
            data ??= new();
            using Stream stream = AssemblyUtils.GetResourceStream(path, false);
            using StreamReader reader = new(stream);
            data.Merge(JObject.Parse(reader.ReadToEnd()));
        }

        return data;
    }
}
