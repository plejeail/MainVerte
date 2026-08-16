# MainVerte agent guide

MainVerte is a C#/.NET Android plant-care app. The current implementation covers a specimen collection, specimen CRUD, local photos, and an initial care-rule MVP. The broader domain remains specification-led, so keep changes narrow and aligned with the documented scope.

## Repository map

```text
MainVerte.AndroidApp (net10.0-android, API 27+) --> MainVerte.Core (net10.0)
MainVerte.Tests       (xUnit, net10.0)           --> MainVerte.Core
MainVerte.Core                                  --> Microsoft.Data.Sqlite
                                                    SQLitePCLRaw.bundle_e_sqlite3
```

- `MainVerte.Core/`: platform-independent domain types, persistence, care scheduling, and shared utilities. It must remain usable and testable without Android.
- `MainVerte.AndroidApp/`: Android UI using AppCompat, AndroidX Fragments, RecyclerView, and Material Components.
- `MainVerte.Tests/`: Core and persistence tests using xUnit.
- `Docs/`: normative domain, database, scheduler, and icon documentation.
- `raw_resources/`: source SVG assets used to produce application artwork and custom icons.
- `MainVerte.sln`: references the three projects above. There is no separate `MainVerte/` residual project.

## Current implementation status

- `CollectionFragment` displays persisted specimen summaries in a responsive grid, loads thumbnail photos, and opens specimen details.
- `MainActivity.ShowAddSpecimen()` opens the specimen creation flow. Until collection selection is implemented, new specimens use the bootstrapped collection with ID `0`.
- `SpecimenDetailsFragment` supports read, create, and edit modes; specimen name and photo editing; deletion; and care-rule cards with rule editing and “do now” rescheduling.
- `Photo.cs` owns Android photo import/capture support, local photo storage, cleanup of pending files, and thumbnail loading.
- `Database` persists specimen data and care rules. `SpecimenEditor` manages the create/edit draft lifecycle.
- `CareScheduler` implements earliest-trigger planning and overdue-rule processing in Core, with focused tests.
- The Android `IPlatform` implementation currently leaves `Publish(...)` and `UpdateSchedulerTriggerTime(...)` as no-ops. System wake-ups, care-event presentation, and notifications are not yet wired.
- Collection selection, species/location management UI, journals/events, alerts, and the wider domain entities described by the specifications are not fully implemented yet.

## Domain and documentation contract

Before changing domain behavior, read both co-canonical documents:

- `Docs/domain_definitions.md`: normative terminology and entity rules.
- `Docs/domain_concept.md`: normative capabilities and workflows.

For persistence changes also read `Docs/db_definitions.md`. For scheduler behavior read `Docs/code_scheduler.md`. For custom icon work read `Docs/ux_icons_rules.md`. Use `TODO.md` as the current roadmap; it is not a substitute for checking the implemented code and tests.

A domain change must update both co-canonical domain documents in the same change when their contract changes. Update the database documentation and migrations when the storage contract changes. Do not duplicate authoritative domain rules in code comments or this guide.

## Core constraints

`MainVerte.Core/Database.cs` owns SQLite. Each `Database` instance starts one `MVDB` background thread, opens one connection, configures foreign keys/WAL/busy timeout, applies embedded migrations, and serializes jobs through a producer/consumer queue.

- Create a `Database`, call its instance method `Initialize(databasePath)` before using database APIs, and dispose it when the owner shuts down.
- Run database work through the typed Core APIs, which use the private `Enqueue(...)` pipeline. Never expose `SqliteConnection` or execute SQL from Android UI code.
- Keep SQL and typed persistence APIs in Core. The internal raw SQL helpers are used by Core tests.
- Migrations live in `MainVerte.Core/Data/Migrations/*.sql` and are embedded by `MainVerte.Core.csproj`. The current migrations are `0000_init.sql`, `0001_bootstrap_default_collection.sql`, and `0002_care_rule.sql`.
- Runtime migration discovery expects the resource prefix `MainVerte.Core.Data.Migrations.`. Migration files use zero-based versions; `DatabaseVersion` is currently hard-coded to `3`, and SQLite `user_version` represents the number of applied migrations. Verify generated resource names when adding a migration.
- Migration `0001` bootstraps gardener `0` and collection `0`; this is the collection used by the current MVP UI.

`MainVerte.Core/Platform.cs` defines the platform abstraction and default implementation. Android registers `AndroidPlatform` from `MainVerte.AndroidApp/Plateform.Android.cs` during `MainActivity` startup. The historical `Plateform` spelling is limited to that Android adapter; do not introduce Android dependencies into Core.

`Require.*` and `Ensure.*` are debug-only assertions. They do not provide Release validation. Use explicit exceptions or other runtime validation for conditions that must hold in Release. `Log.Error(...)` reports through the platform and calls `Debug.Fail`; it is not a replacement for throwing a required exception.

`System.Linq` is forbidden by `MainVerte.Core/BannedSymbols.txt`, and analyzer rule `RS0030` is treated as an error. Do not add LINQ or Android dependencies to Core. Follow the existing C# style in the surrounding file.

## Android constraints

`MainActivity`:

- installs the splash screen and registers the Android platform implementation;
- installs crash and unobserved-task handlers;
- initializes the shared `Services.Database` at `Application.Context.GetDatabasePath("mainverte.db")`;
- cleans pending photo files; and
- hosts `activity_main.xml`, with the collection as the root screen.

`AndroidGenerateLayoutBindings` is enabled. Preserve the `xamarin:managedType` declarations in layouts when they are needed to resolve AndroidX or Material types.

- Keep database access in Core; Android fragments should call typed APIs or Core editors.
- The collection root does not enable back navigation. Detail screens request a back button and use the activity back-navigation callback to protect unsaved or busy edits. `ToolbarLeftButton.Logo` currently does not install a visible toolbar icon; verify intended behavior before changing toolbar navigation.
- `SpecimenViewHolder` and `SpecimenAdapter` are implemented in `CollectionFragment.cs`; do not create a duplicate holder in a separate file.
- Custom Android drawable icons should follow `Docs/ux_icons_rules.md` and the existing `Resources/drawable/` naming conventions.

## Tests and validation

Add focused xUnit coverage for new Core, persistence, or scheduler behavior. Database tests use isolated paths under `Path.GetTempPath()/MainVerte.Tests/<guid>/mv.db`; retain that pattern and dispose every `Database` instance.

Run the smallest relevant validation from the repository root:

```powershell
# Core, persistence, or scheduler changes
dotnet test MainVerte.Tests\MainVerte.Tests.csproj

# Android changes
dotnet build MainVerte.AndroidApp\MainVerte.AndroidApp.csproj

# Cross-project changes
dotnet build MainVerte.sln
```

The repository targets .NET 10. Check the installed SDK and Android workload before attributing toolchain failures to source changes.
