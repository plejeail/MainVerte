namespace MainVerte;


using System;

#if ANDROID
using Android.App;
#endif

static class Platform
{
    public static void LogError(string message) {
#if ANDROID
        Android.Util.Log.Error("MainVerte", message);
#else
        Console.WriteLine($"ERR: MainVerte: {message}");
#endif
    }

    public static void LogWarning(string message) {
#if ANDROID
        Android.Util.Log.Warn("MainVerte", message);
#else
        Console.WriteLine($"WRN: MainVerte: {message}");
#endif
    }

    public static void LogInfo(string message) {
#if ANDROID
        Android.Util.Log.Info("MainVerte", message);
#else
        Console.WriteLine($"INF: MainVerte: {message}");
#endif
    }

    public static void LogVerbose(string message) {
#if ANDROID
        Android.Util.Log.Verbose("MainVerte", message);
#else
        Console.WriteLine($"VER: MainVerte: {message}");
#endif
    }

    public static void LogDebug(string message) {
#if ANDROID
        Android.Util.Log.Debug("MainVerte", message);
#else
        Console.WriteLine($"DBG: MainVerte: {message}");
#endif
    }

    public static string ApplicationPath() {
#if ANDROID
        return Application.Context.FilesDir?.AbsolutePath ?? String.Empty;
#else
        return ".";
#endif
    }
}
