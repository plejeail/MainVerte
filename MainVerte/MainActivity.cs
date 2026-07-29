namespace MainVerte;

#if ANDROID
using System;
using System.Threading.Tasks;
using _Microsoft.Android.Resource.Designer;
using Android.App;
using Android.OS;
using Android.Runtime;
using Activity = Android.App.Activity;

static class Services
{
    public static bool Initialized = false;
    public static readonly Database Database = new();

    public static void EnsureInitialized() {
        Require.True(!Initialized);

        Initialized = true;

        string? dbPath = Application.Context.GetDatabasePath("mainverte.db")?.AbsolutePath;
        if (dbPath is null) {
            throw new InvalidOperationException("Database path is null");
        }

        Database.Initialize(dbPath);
    }
}

[Activity(Label = "@string/app_name", MainLauncher = true)]
sealed class MainActivity : Activity
{
    protected override void OnCreate(Bundle? savedInstanceState)
    {
        base.OnCreate(savedInstanceState);
        // Set our view from the "main" layout resource
        SetContentView(ResourceConstant.Layout.activity_main);
    }

    private static void OnUnhandledException(object? sender, UnhandledExceptionEventArgs e) {
        if (e.ExceptionObject is Exception ex) {
            CrashReport.Write(ex);
        }
    }

    private static void OnUnobservedTaskException(object? sender, UnobservedTaskExceptionEventArgs e) {
        CrashReport.Write(e.Exception);
    }

    private static void OnAndroidException(object? sender, RaiseThrowableEventArgs e) {
        CrashReport.Write(e.Exception);
    }
}
#else
static class Program
{
    public static void Main(string[] args) {}
}
#endif
