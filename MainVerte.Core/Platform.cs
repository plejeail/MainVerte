//! MainVerte.Core/platform.cs -------------------------------------------------
//!
//! PLATFORM ABSTRACTION
//!
//! Defines the platform-dependent services used by MainVerte.Core.
//!
//! Core code expresses platform-independent intents through Platform, while
//! the host application decides how those intents are implemented on the
//! current operating system.
//!
//! A default platform-independent implementation is provided so that Core can
//! operate without a registered host implementation. A host application may
//! override platform behavior by implementing IPlatform and registering the
//! implementation with Platform.SetImplementation(...) during startup.
//!
//! ---------------------------------------------------------------------------
using System.Diagnostics;
using System.Runtime.CompilerServices;

namespace MainVerte.Core;


public enum LogLevel { Debug, Info, Warning, Error, }

public enum UserFeedbackKind { Info, Success, Warning, Failure, }

/// <summary>
/// Defines the platform-dependent services required by MainVerte.Core.
/// </summary>
/// <remarks>
/// Implementations translate platform-independent requests from Core into
/// behavior appropriate for the host platform.
/// </remarks>
public interface IPlatform
{
    /// <summary> Writes a message to the platform logging system. </summary>
    /// <param name="message">The message to write.</param>
    /// <param name="level">The severity level of the message.</param>
    public void LogMessage(string message, LogLevel level);

    /// <summary> Gets the path of the application's local storage directory. </summary>
    /// <returns> A path where the Core lib can store application-local files. </returns>
    public string ApplicationPath();

    /// <summary> Presents feedback to the user. </summary>
    /// <param name="message">The user-facing message to present.</param>
    /// <param name="kind">
    /// The intent of the feedback, allowing the host platform to choose an
    /// appropriate presentation.
    /// </param>
    public void UserFeedback(string message, UserFeedbackKind kind);
}

sealed class DefaultPlatform : IPlatform
{
    public void LogMessage(string message, LogLevel level) {
        Console.WriteLine($"{level}: {message}");
    }

    public string ApplicationPath() {
        return AppContext.BaseDirectory;
    }

    public void UserFeedback(string message, UserFeedbackKind kind) {}
}

public static class Platform
{
    private static IPlatform _platform = new DefaultPlatform();

    /// <summary> Replace dedfault implementation with a platform-specific implementation. </summary>
    /// <param name="platform">The implementation to use for platform calls.</param>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void SetImplementation(IPlatform platform) {
        Require.NotNull(platform);
        _platform = platform;
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void LogDebug(string message) {
        _platform.LogMessage(message,  LogLevel.Debug);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void LogInfo(string message) {
        _platform.LogMessage(message,  LogLevel.Info);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void LogWarning(string message) {
        _platform.LogMessage(message, LogLevel.Warning);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void LogError(string message) {
        _platform.LogMessage(message, LogLevel.Error);
        Debug.Fail(message);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static string ApplicationPath() {
        return _platform.ApplicationPath();
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void UserFeedbackSuccess(string message) {
        _platform.UserFeedback(message,  UserFeedbackKind.Success);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void UserFeedbackFailure(string message) {
        _platform.UserFeedback(message,  UserFeedbackKind.Failure);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void UserFeedbackInfo(string message) {
        _platform.UserFeedback(message,  UserFeedbackKind.Info);
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void UserFeedbackWarning(string message) {
        _platform.UserFeedback(message,  UserFeedbackKind.Warning);
    }
}
