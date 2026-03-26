using System.IO;
using System.Reflection;

namespace TRX_InstallerLib.Utils;

public static class AssemblyUtils
{
    public static readonly string _resourcePathFormat = "pack://application:,,,/{0};component/Resources/{1}";

    private static Assembly? GetReferencedAssembly(bool local)
    {
        return local ? Assembly.GetExecutingAssembly() : Assembly.GetEntryAssembly();
    }

    public static Stream GetResourceStream(string relativePath, bool local)
    {
        return GetReferencedAssembly(local)?.GetManifestResourceStream(GetAbsolutePath(relativePath, local))!;
    }

    public static bool ResourceExists(string relativePath, bool local)
    {
        return GetReferencedAssembly(local)?.GetManifestResourceNames()
            .Contains(GetAbsolutePath(relativePath, local)) ?? false;
    }

    public static string GetAbsolutePath(string relativePath, bool local)
    {
        return $"{GetReferencedAssembly(local)!.GetName().Name}.{relativePath}";
    }

    public static string GetEmbeddedResourcePath(string resource)
    {
        return string.Format(_resourcePathFormat, Assembly.GetEntryAssembly()!.GetName().Name, resource);
    }
}
