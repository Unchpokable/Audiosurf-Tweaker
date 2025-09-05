namespace Tweaker.Settings;

public class AppSettings
{
    public string DefaultGamePath { get; set; } = string.Empty;
    public int MaxConcurrentOperations { get; set; } = 4;
    public string TempDirectory { get; set; } = string.Empty;
}

public class FileFormats
{
    public string[] SupportedArchives { get; set; } = [];
    public string[] SupportedImages { get; set; } = [];
}