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

using System.Runtime.CompilerServices;

namespace MainVerte.Core;


public interface IPlatform
{
    /// <summary>
    ///
    /// </summary>
    /// <param name="message"></param>
    public void LogError(string message);

    /// <summary>
    ///
    /// </summary>
    /// <param name="message"></param>
    public void LogWarning(string message);

    /// <summary>
    ///
    /// </summary>
    /// <param name="message"></param>
    public void LogInfo(string message);

    /// <summary>
    ///
    /// </summary>
    /// <param name="message"></param>
    public void LogDebug(string message);

    /// <summary> Get the path to the application data files</summary>
    /// <returns></returns>
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

    /// <summary> Replace dedfault implementation with a platform-specific implementation. </summary>
    /// <param name="platform">The implementation to use for platform calls.</param>
    public static void SetImplementation(IPlatform platform) {
        _platform = platform;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void LogInfo(string message) {
        _platform.LogInfo(message);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void LogDebug(string message) {
        _platform.LogDebug(message);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void LogWarning(string message) {
        _platform.LogWarning(message);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void LogError(string message) {
        _platform.LogError(message);
    }

    internal static string ApplicationPath() {
        return _platform.ApplicationPath();
    }
}
