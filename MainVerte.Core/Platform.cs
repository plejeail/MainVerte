//! MainVerte.Core/platform.cs -------------------------------------------------
//!
//! PLATFORM SPECIFIC CODE
//!
//! The default implementation is platform-independent.
//! A host platform can override selected behavior by implementing IPlatform and
//! registering its implementation with Platform.SetImplementation(...)
//! during application startup.
//!
//! ---------------------------------------------------------------------------
namespace MainVerte.Core;


public interface IPlatform
{
    public void   LogError(string message);
    public void   LogWarning(string message);
    public void   LogInfo(string message);
    public void   LogVerbose(string message);
    public void   LogDebug(string message);
    public string ApplicationPath();
}

sealed class DefaultPlatform : IPlatform
{
    public void LogError(string message) {
        Console.WriteLine($"ERR: MainVerte: {message}");
    }

    public void LogWarning(string message) {
        Console.WriteLine($"WRN: MainVerte: {message}");
    }

    public void LogInfo(string message) {
        Console.WriteLine($"INF: MainVerte: {message}");
    }

    public void LogVerbose(string message) {
        Console.WriteLine($"VER: MainVerte: {message}");
    }

    public void LogDebug(string message) {
        Console.WriteLine($"DBG: MainVerte: {message}");
    }

    public string ApplicationPath() {
        return ".";
    }
}

public static class Platform
{
    private static IPlatform _platform = new DefaultPlatform();

    public static void SetImplementation(IPlatform platform) {
        _platform = platform;
    }

    internal static void LogError(string message) {
        _platform.LogError(message);
    }

    internal static void LogWarning(string message) {
        _platform.LogWarning(message);
    }

    internal static void LogInfo(string message) {
        _platform.LogInfo(message);
    }

    internal static void LogVerbose(string message) {
        _platform.LogVerbose(message);
    }

    internal static void LogDebug(string message) {
        _platform.LogDebug(message);
    }

    internal static string ApplicationPath() {
        return _platform.ApplicationPath();
    }
}
