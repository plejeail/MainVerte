//! MainVerte.Core/Utils.cs ---------------------------------------------------
//!
//! TOOLS AND UTILITARY METHODS
//!
//! Toolbox that may be used anywhere, unrelated to MainVerte.
//! ---------------------------------------------------------------------------
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;

namespace MainVerte.Core;


public static class Require
{
    [Conditional("DEBUG")]
    public static void True(bool condition, [CallerArgumentExpression(nameof(condition))] string? expression = null) {
        if (!condition) {
            Debug.Fail($"Precondition failed: {expression}");
        }
    }

    [Conditional("DEBUG")]
    public static void NotNull<T>(T? value, [CallerArgumentExpression(nameof(value))] string? expression = null) {
        if (value == null) {
            Debug.Fail($"Precondition failed: {expression} is null");
        }
    }

    [Conditional("DEBUG")]
    public static void NotEmpty(string? value,
                                [CallerArgumentExpression(nameof(value))] string? expression = null)
    {
        if (String.IsNullOrEmpty(value)) {
            Debug.Fail($"Precondition failed: {expression} must not be null or empty.");
        }
    }

    [Conditional("DEBUG")]
    public static void NotEmpty<T>(IReadOnlyCollection<T>? value,
                                   [CallerArgumentExpression(nameof(value))] string? expression = null)
    {
        NotNull(value);
        if (value!.Count == 0) {
            Debug.Fail($"Precondition failed: {expression} must not be empty.");
        }
    }
}

public static class Ensure
{

    [Conditional("DEBUG")]
    public static void True(bool condition, [CallerArgumentExpression(nameof(condition))] string? expression = null) {
        if (!condition) {
            Debug.Fail($"Postcondition failed: {expression}");
        }
    }

    [Conditional("DEBUG")]
    public static void NotNull<T>(T? value, [CallerArgumentExpression(nameof(value))] string? expression = null) {
        if (value == null) {
            Debug.Fail($"Postcondition failed: {expression} is null");
        }
    }

    [Conditional("DEBUG")]
    public static void NotEmpty(string? value,
                                [CallerArgumentExpression(nameof(value))] string? expression = null)
    {
        if (String.IsNullOrEmpty(value)) {
            Debug.Fail($"Precondition failed: {expression} must not be null or empty.");
        }
    }

    [Conditional("DEBUG")]
    public static void NotEmpty<T>(IReadOnlyCollection<T>? value,
                                   [CallerArgumentExpression(nameof(value))] string? expression = null)
    {
        NotNull(value);
        if (value!.Count == 0) {
            Debug.Fail($"Precondition failed: {expression} must not be empty.");
        }
    }
}

public static class Log
{
    public static void Info(string message) {
        Platform.LogInfo(message);
    }

    public static void Verbose(string message) {
        Platform.LogVerbose(message);
    }

    public static void Debug(string message) {
        Platform.LogDebug(message);
    }

    public static void Warn(string message) {
        Platform.LogWarning(message);
    }

    public static void Error(string message) {
        System.Diagnostics.Debug.Fail(message);
        Platform.LogError(message);
    }
}

public static class CrashReport
{
    public static void Write(Exception ex) {
        string applicationPath = Platform.ApplicationPath();
        string diagnosticsPath = Path.Combine(applicationPath, "diagnostics");
        string reportPath = Path.Combine(diagnosticsPath, "crash_report_pending.txt");
        try {
            Directory.CreateDirectory(diagnosticsPath);
        } catch (Exception dirEx) {
            Platform.LogError($"Failed to create directory for crash report: {dirEx}");
            return;
        }

        string report = $"""
                         timestamp:   {DateTimeOffset.UtcNow.ToUnixTimeSeconds()}
                         description: {ex.Message}
                         stacktrace:
                         {ex.StackTrace ?? "<no stacktrace>"}
                         """;

        File.WriteAllText(reportPath, report);
    }

    public static void ProcessPending() {
        string applicationPath = Platform.ApplicationPath();
        string diagnosticsPath = Path.Combine(applicationPath, "diagnostics");
        string reportPath = Path.Combine(diagnosticsPath, "crash_report_pending.txt");
        if (!File.Exists(reportPath)) {
            return;
        }

        try {
            string[] lines = File.ReadAllLines(reportPath);
            long? timestamp = ExtractTimestamp(lines);
            if (timestamp == null) {
                Platform.LogWarning("Missing or invalid crash report timestamp.");
#if !DEBUG
                File.Delete(reportPath);
#endif
                return;
            }

            string archivedReportPath = Path.Combine(diagnosticsPath, $"crash_report_{timestamp}.txt");
            File.Move(reportPath, archivedReportPath, true);
        } catch (Exception ex) {
            Platform.LogWarning($"Failed to process pending crash report: {ex}");
#if !DEBUG
            try {
                if (File.Exists(reportPath)) {
                    File.Delete(reportPath);
                }
            } catch (Exception e) {
                Platform.LogWarning("Unable to remove corrupted report");
            }
#endif
        }
    }

    private static long? ExtractTimestamp(string[] lines) {
        const string prefix = "timestamp: ";

        foreach (string line in lines) {
            if (!line.StartsWith(prefix, StringComparison.Ordinal)) {
                continue;
            }

            string value = line[prefix.Length..].Trim();

            if (Int64.TryParse(value, out long unixTimestamp)) {
                return unixTimestamp;
            }

            break;
        }

        return null;
    }
}
