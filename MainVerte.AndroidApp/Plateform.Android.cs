using System;
using Android.App;
using Android.Graphics;
using Android.Views;
using AndroidX.Core.Content;
using Google.Android.Material.Snackbar;
using MainVerte.Core;

namespace MainVerte.AndroidApp;

static class Feedback
{
    public static void Send(Activity activity, string message, FeedbackKind kind) {
        if (activity.IsFinishing || activity.IsDestroyed) {
            return;
        }

        activity.RunOnUiThread(() => {
            if (activity.IsFinishing || activity.IsDestroyed) {
                return;
            }

            View? content = activity.FindViewById(Android.Resource.Id.Content);
            if (content == null) {
                return;
            }

            var snackbar = Snackbar.Make(content, message, Snackbar.LengthLong);
            snackbar.View.Clickable = true;
            snackbar.View.Click += (_, _) => snackbar.Dismiss();
            snackbar.View.SetBackgroundColor(GetFeedbackColor(activity, kind));
            snackbar.Show();
        });
    }

    private static Color GetFeedbackColor(Activity activity, FeedbackKind kind) {
        Require.IsInRange(kind);

        int colorResource = Resource.Color.feedback_info;
        switch (kind) {
        case FeedbackKind.Success: colorResource = Resource.Color.feedback_success; break;
        case FeedbackKind.Failure: colorResource = Resource.Color.feedback_failure; break;
        case FeedbackKind.Info:    colorResource = Resource.Color.feedback_info;    break;
        case FeedbackKind.Warning: colorResource = Resource.Color.feedback_warning; break;
        }

        return new Color(ContextCompat.GetColor(activity, colorResource));
    }
}

sealed class AndroidPlatform : IPlatform
{
    private readonly WeakReference<Activity> _activity;

    public AndroidPlatform(Activity activity) {
        Require.NotNull(activity);
        _activity = new WeakReference<Activity>(activity);
    }

    public void LogMessage(string message, LogLevel level) {
        switch (level) {
        case LogLevel.Debug:   Android.Util.Log.Debug("MainVerte", message); break;
        case LogLevel.Info:    Android.Util.Log.Info("MainVerte", message);  break;
        case LogLevel.Warning: Android.Util.Log.Warn("MainVerte", message);  break;
        case LogLevel.Error:   Android.Util.Log.Error("MainVerte", message); break;
        }
    }

    public string ApplicationPath() {
        return Application.Context.FilesDir?.AbsolutePath ?? String.Empty;
    }

    public void UserFeedback(string message, FeedbackKind kind) {
        if (_activity.TryGetTarget(out Activity? activity)) {
            Feedback.Send(activity, message, kind);
        }
    }

    public void Publish(MainVerteEvent payload) {}

    public void UpdateSchedulerTriggerTime(DateTimeOffset newDate) {}
}
