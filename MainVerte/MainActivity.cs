namespace MainVerte;

#if ANDROID
using System;
using System.Threading.Tasks;
using _Microsoft.Android.Resource.Designer;
using Android.App;
using Android.OS;
using Android.Runtime;
using AndroidX.AppCompat.App;
using AndroidX.Core.App;

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
sealed class MainActivity : AppCompatActivity
{
    protected override void OnCreate(Bundle? savedInstanceState)
    {
        base.OnCreate(savedInstanceState);
        SetContentView(ResourceConstant.Layout.activity_main);

        if (savedInstanceState is null) {
            SupportFragmentManager.BeginTransaction()
                                  .SetReorderingAllowed(true)
                                  .Add(ResourceConstant.Id.main_fragment_container,
                                       new CollectionFragment())
                                  .Commit();
        }
    }

    public void OpenAddPlant()
    {
        SupportFragmentManager
            .BeginTransaction()
            .SetReorderingAllowed(true)
            .Replace(
                     ResourceConstant.Id.main_fragment_container,
                     new AddPlantFragment())
            .AddToBackStack(null)
            .Commit();
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
