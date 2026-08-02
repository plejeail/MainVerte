# MainVerte agent guide

MainVerte is a native C#/.NET Android plant-care app. Keep changes narrow: the codebase is early-stage and its domain is still specification-led.

## Repository map

```text
MainVerteAndroid (net10.0-android) --> MainVerteCore (net10.0)
MainVerteTests   (xUnit/net10.0)   --> MainVerteCore
MainVerteCore                       --> Microsoft.Data.Sqlite
```

- `MainVerteAndroid/`: Android UI using AppCompat, AndroidX Fragments, RecyclerView, and Material Components.
- `MainVerteCore/`: platform-independent domain, persistence, and shared utilities. It must remain usable and testable without Android.
- `MainVerteTests/`: Core tests only.
- `Docs/`: normative domain specifications and roadmap.
- `MainVerte/`: residual directory not referenced by `MainVerte.sln`; do not add code there without confirming its purpose.

## Domain contract

Before changing domain behavior, read both:

- `Docs/domain_definitions.md`: normative terminology and entity rules.
- `Docs/domain_concept.md`: normative capabilities and workflows.

They are co-canonical. A domain change must update both in the same change; disagreement between them is a specification defect. Also read `Docs/db_definitions.md` for persistence changes and `Docs/dev_road_map.md` when feature scope is unclear.

Do not duplicate domain rules in code comments or this guide when the specification is the authoritative source.

## Core constraints

`MainVerteCore/Database.cs` owns SQLite. Each `Database` instance starts one `MVDB` thread, opens one connection, applies embedded migrations, and serializes jobs through a producer/consumer queue.

- Call `Database.Initialize(databasePath)` before queries.
- Run all database work through `Enqueue(...)`; never expose `SqliteConnection` or execute SQL from Android UI code.
- Keep SQL and typed persistence APIs in Core.
- Migrations belong in `MainVerteCore/Data/Migrations/*.sql` as embedded resources. Runtime discovery expects the `MainVerte.Data.Migrations.` prefix; verify generated resource names when adding one.
- No migration files currently exist. `DatabaseVersion` is still hard-coded.

`MainVerteCore/Plateform.cs` (intentional current spelling) isolates platform logging and application paths with conditional compilation. Do not introduce Android dependencies elsewhere in Core.

`Require.*` and `Ensure.*` compile only in `DEBUG`; do not use them for validation required in Release. `Log.Error(...)` logs and throws `InvalidOperationException`.

`System.Linq` is banned by `BannedSymbols.txt`, and analyzer rule `RS0030` is an error. Do not use LINQ.

Avoid switch pattern matching; prefer regular `switch` statements or `if`/`else` branches. Avoid `x is null` and `x is not null`; prefer `x == null` and `x != null`. Avoid the `??` operator when the expression does not fit on one line.

## Android constraints

`MainActivity` initializes the shared `Services` instance and database at `Application.Context.GetDatabasePath("mainverte.db")`, installs crash handlers, and hosts fragments in `activity_main.xml`.

`AndroidGenerateLayoutBindings` is enabled. Preserve layout `xamarin:managedType` declarations needed to resolve AndroidX/Material types.

Current incomplete areas:

- `CollectionFragment` configures a grid RecyclerView but has no adapter/data source.
- `ShowAddSpecimen()` is a stub.
- `SpecimenDetailsFragment` displays placeholder data.
- `SpecimenViewHolder.cs` is empty; the actual class is in `CollectionFragment.cs`.
- The collection root currently requests a back button through `ToolbarConfiguration`; confirm intended navigation before changing it.

## Tests and validation

Add focused xUnit coverage for new Core behavior. Database tests use isolated paths under `Path.GetTempPath()/MainVerte.Tests/<guid>/mv.db`; retain that pattern.

Run the smallest relevant validation from the repository root:

```powershell
# Core or persistence changes
dotnet test MainVerteTests\MainVerteTests.csproj

# Android changes
dotnet build MainVerteAndroid\MainVerteAndroid.csproj

# Cross-project changes
dotnet build MainVerte.sln
```

The repository targets .NET 10; check the installed SDK before attributing toolchain failures to source changes.
