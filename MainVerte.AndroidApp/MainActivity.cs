using System;
using System.Threading.Tasks;
using Android.App;
using Android.OS;
using Android.Runtime;
using Android.Views;
using AndroidX.Activity;
using AndroidX.AppCompat.App;

using MainVerte.Core;

using Android_Resource = Android.Resource;

namespace MainVerte.AndroidApp;


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
        PhotoStorage.CleanupPendingFiles();
    }
}

[Activity(Label = "@string/app_name",
          MainLauncher = true,
          Theme =  "@style/MainVerteTheme")]
sealed class MainActivity : AppCompatActivity
{
    private Binding.activity_main? _binding;
    private ToolbarMenuAction[] _toolbarActions = Array.Empty<ToolbarMenuAction>();

    protected override void OnCreate(Bundle? savedInstanceState)
    {
        if (!Services.Initialized) { // Run once
            Platform.SetImplementation(new AndroidPlatform());
            CrashReport.ProcessPending();

            // Catch unhandled exceptions
            AppDomain.CurrentDomain.UnhandledException  += OnUnhandledException;
            TaskScheduler.UnobservedTaskException       += OnUnobservedTaskException;
            AndroidEnvironment.UnhandledExceptionRaiser += OnAndroidException;

            Services.EnsureInitialized();
        }

        base.OnCreate(savedInstanceState);
        OnBackPressedDispatcher.AddCallback(this, new MainActivityBackCallback(this));
        // Set our view from the "main" layout resource
        SetContentView(Resource.Layout.activity_main);

        _binding = new Binding.activity_main(FindViewById(Android_Resource.Id.Content)!);
        SetSupportActionBar(_binding.main_toolbar);

        if (savedInstanceState == null) {
            ShowCollection();
        }
    }

    public override bool OnSupportNavigateUp()
    {
        return HandleBackNavigation();
    }

    private bool HandleBackNavigation()
    {
        AndroidX.Fragment.App.Fragment? currentFragment = SupportFragmentManager.FindFragmentById(Resource.Id.main_fragment_container);
        if (currentFragment is SpecimenDetailsFragment specimenDetails
            && specimenDetails.HandleBackNavigation()) {
            return true;
        }

        if (SupportFragmentManager.BackStackEntryCount == 0) {
            return false;
        }

        SupportFragmentManager.PopBackStack();
        return true;
    }

    private sealed class MainActivityBackCallback(MainActivity activity) : OnBackPressedCallback(true)
    {
        private readonly MainActivity _activity = activity;

        public override void HandleOnBackPressed()
        {
            if (_activity.HandleBackNavigation()) {
                return;
            }

            Enabled = false;
            _activity.OnBackPressedDispatcher.OnBackPressed();
            Enabled = true;
        }
    }

    public void ShowCollection() {
        CollectionFragment fragment = new();
        SupportFragmentManager.BeginTransaction()
                              .SetReorderingAllowed(true)
                              .Replace(Resource.Id.main_fragment_container, fragment)
                              .Commit();

        ConfigureToolbar(fragment.ToolbarConfiguration);
    }

    public void ShowAddSpecimen() {
        var fragment = SpecimenDetailsFragment.ForNewSpecimen(new MainVerteId(0));
        SupportFragmentManager.BeginTransaction()
                              .SetReorderingAllowed(true)
                              .Replace(Resource.Id.main_fragment_container, fragment)
                              .AddToBackStack(null)
                              .Commit();
    }

    internal void ConfigureToolbar(ToolbarConfiguration toolbar, ToolbarMenuAction[]? actions = null)
    {
        Require.NotNull(SupportActionBar);

        SupportActionBar!.Title = GetString(toolbar.TitleResourceId);
        SupportActionBar!.SetDisplayHomeAsUpEnabled(toolbar.ShowBackButton);
        _toolbarActions = actions ?? Array.Empty<ToolbarMenuAction>();
        InvalidateOptionsMenu();
    }

    public override bool OnCreateOptionsMenu(IMenu? menu) {
        if (menu is null) {
            return false;
        }

        foreach (ToolbarMenuAction action in _toolbarActions) {
            IMenuItem item = menu.Add(IMenu.None, action.Id, IMenu.None, action.Title)
                          ?? throw new InvalidOperationException("Could not create toolbar menu item.");
            item.SetIcon(action.IconResourceId);
            item.SetShowAsAction(ShowAsAction.Always);
        }

        return true;
    }

    public override bool OnOptionsItemSelected(IMenuItem item) {
        foreach (ToolbarMenuAction action in _toolbarActions) {
            if (action.Id != item.ItemId) {
                continue;
            }

            action.Execute();
            return true;
        }

        return base.OnOptionsItemSelected(item);
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

sealed record ToolbarConfiguration(int TitleResourceId, bool ShowBackButton);
sealed record ToolbarMenuAction(int Id, string Title, int IconResourceId, Action Execute);
