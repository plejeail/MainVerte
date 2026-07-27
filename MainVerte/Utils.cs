namespace MainVerte;

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Runtime.CompilerServices;

static class Require
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

static class Ensure
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

static class Log
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

    [DoesNotReturn]
    public static void Error(string message) {
        Platform.LogError(message);
        throw new InvalidOperationException(message);
    }
}
