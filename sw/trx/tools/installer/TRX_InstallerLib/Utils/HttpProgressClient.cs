using System.IO;
using System.Net.Http;

namespace TRX_InstallerLib.Utils;

public class HttpProgressClient
{
    public delegate void ProgressChangedHandler(long totalBytesToReceive, long bytesReceived);
    public event ProgressChangedHandler? DownloadProgressChanged;

    public async Task<byte[]> DownloadDataTaskAsync(Uri uri)
    {
        using HttpClient client = new();
        client.DefaultRequestHeaders.CacheControl = new()
        {
            NoCache = true
        };

        HttpResponseMessage response = await client.GetAsync(uri, HttpCompletionOption.ResponseHeadersRead);
        response.EnsureSuccessStatusCode();

        long totalBytes = response.Content.Headers.ContentLength ?? 0;

        using Stream contentStream = await response.Content.ReadAsStreamAsync();
        return await ProcessContentStream(totalBytes, contentStream);
    }

    private async Task<byte[]> ProcessContentStream(long totalBytes, Stream contentStream)
    {
        long totalBytesRead = 0;
        byte[] buffer = new byte[8192];

        using MemoryStream outputStream = new();
        while (true)
        {
            int bytesRead = await contentStream.ReadAsync(buffer);
            if (bytesRead == 0)
            {
                break;
            }

            await outputStream.WriteAsync(buffer.AsMemory(0, bytesRead));
            totalBytesRead += bytesRead;

            DownloadProgressChanged?.Invoke(totalBytes, totalBytesRead);
        }

        return outputStream.ToArray();
    }
}
