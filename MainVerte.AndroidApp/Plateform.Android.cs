using System;
using Android.App;
using MainVerte.Core;

namespace MainVerte.AndroidApp;


sealed class AndroidPlatform : IPlatform
{
    public void LogError(string message) {
        Android.Util.Log.Error("MainVerte", message);
    }

    public void LogWarning(string message) {
        Android.Util.Log.Warn("MainVerte", message);
    }

    public void LogInfo(string message) {
        Android.Util.Log.Info("MainVerte", message);
    }

    public void LogVerbose(string message) {
        Android.Util.Log.Verbose("MainVerte", message);
    }

    public void LogDebug(string message) {
        Android.Util.Log.Debug("MainVerte", message);
    }

    public string ApplicationPath() {
        return Application.Context.FilesDir?.AbsolutePath ?? String.Empty;
    }
}
