# MainVerte gardening app

MainVerte is a gardening journal for Windows desktop and Android (ARM64/x86_64),
written in C++23 with Qt Quick/QML and SQLite.

## Prerequisites

- Git, including submodule support.
- CMake >= 3.25 and a C/C++ toolchain with C++23 support.
- Qt >= 6.12 with Qt Quick and Qt Quick Controls 2, built for the selected
  compiler and target platform.
- A host Python interpreter >= 3.9 for translation generation, including when
  cross-compiling for Android. No third-party Python packages are needed.
- For Android: a Qt Android kit and its compatible Android SDK, NDK and JDK.
  Configure these in Qt Creator before selecting an ARM64 or x86_64 kit.

SQLite sources are included in the repository; doctest is a Git submodule.
Neither needs a separate system installation.

## Initialize the checkout

From the repository root, initialize the submodule after cloning:

```powershell
git submodule update --init --recursive
```

Run this command again after pulling a change that updates the submodule revision.

## Build and test

Run the following commands from the repository root in an environment configured
for your Qt desktop kit and compiler. For a multi-config generator such as Visual
Studio or Ninja Multi-Config:

```powershell
cmake -S . -B build/desktop -DBUILD_TESTING=ON
cmake --build build/desktop --config Debug --target appMainVerte mainverte_core_tests
ctest --test-dir build/desktop -C Debug --output-on-failure --no-tests=error
```

If CMake cannot find Qt, pass `-DCMAKE_PREFIX_PATH="<Qt kit directory>"` to the
configuration command, replacing the placeholder with your kit's installation
directory. Keep machine-specific paths out of version control.

For a single-config generator such as Ninja, use:

```powershell
cmake -S . -B build/desktop -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build/desktop --target appMainVerte mainverte_core_tests
ctest --test-dir build/desktop --output-on-failure --no-tests=error
```

Use only one of these configurations for a given build directory. Keep separate
build directories for each platform, compiler and Android ABI; never reuse a
CMake cache across kits. The application target is `appMainVerte`; the Core
library target is `mainverte_core`.

To build and run on Android, open the root `CMakeLists.txt` in Qt Creator, select
the Android kit and ABI, and use a separate build directory (for example,
`build/android-arm64`). Build `appMainVerte` and run it on a matching device or
emulator. Desktop builds and tests do not validate Android deployment.

## Application data

The application stores its SQLite database as `database.db` in the directory
returned by `QStandardPaths::AppLocalDataLocation`, using the application name
`MainVerte`. This location is independent of the working and build directories.

- Windows: normally `%LOCALAPPDATA%\MainVerte\database.db`.
- Android: `database.db` is stored in the app's private data directory, whose
  exact path is determined by Qt and the installed application package.

## SQL migrations

Schema migrations live in `core/sql/`. CMake embeds them in
`core/generated/sql_scripts.hpp` under the build directory; no external SQL
files need to be deployed with the application. SQL changes trigger CMake
reconfiguration when building.

Migration filenames use consecutive, zero-based prefixes with consistent
padding: `0000_init.sql`, `0001_add_plants.sql`, etc. CMake sorts filenames and
checks that the sequence starts at zero without gaps or duplicate indices.
Files that do not match `<number>_<description>.sql` are ignored.

When the database opens, Core reads SQLite's `PRAGMA user_version` and applies
the remaining scripts in order. Script `0000` upgrades version 0 to 1, script
`0001` upgrades version 1 to 2, and so on. The target version is the number of
embedded scripts; there is no separate version constant to update manually.

Each script and its version update run in one transaction. On failure, Core
attempts to roll back that migration and initialization fails. Earlier committed
migrations remain applied. Databases newer than the application's target version
are rejected; downgrades are not supported.

To change the schema:

1. Add the next numbered SQL file instead of modifying a migration already
   shipped. Existing databases do not replay completed migrations.
2. Preserve existing user data. Leave transaction boundaries and
   `PRAGMA user_version` updates to Core; do not delete or recreate the database.
3. Add regression tests under `core/tests/` covering upgrades from the previous
   schema with existing data, as well as fresh database creation. Migration
   transaction tests are in `core/tests/test_database_migration.cpp`.
4. Rebuild and run the Core tests:

```powershell
cmake --build build/desktop --config Debug --target mainverte_core_tests
ctest --test-dir build/desktop -C Debug -R '^mainverte_core_tests$' --output-on-failure --no-tests=error
```

For a single-config generator, omit `--config Debug` and `-C Debug`.

## Embedded translations

Building the Qt application requires a host Python interpreter >= 3.9, including
for Android builds. No third-party Python packages are needed.

The ordered `DICTIONARIES` list in `app_qt/CMakeLists.txt` defines the languages.
Its first dictionary defines all text IDs and their numeric order. Other
dictionaries must contain exactly the same IDs, in any order. Numeric IDs are
internal build indices and must not be persisted.

Each `MainVerte_<language>.dict` is a UTF-8 CSV without a header, with exactly two
columns: C++ identifier and text. Use IDs such as `App_Title`; `Count`, C++
keywords and reserved identifiers are rejected. Duplicate IDs and empty
dictionaries are errors. Empty translations are allowed. CSV quoting supports
commas, quotes (doubled inside quoted fields), and multiline text:

```csv
App_Title,MainVerte
Collection_Title,"Collection, plants and seeds"
Parameters_Title,Parameters
```

CMake runs `tools/generate_translations.py` when a dictionary or the generator
changes. Headers are written under the build directory at
`app_qt/generated/`; unchanged content is not rewritten. To generate
them explicitly:

```powershell
cmake --build build/desktop --config Debug --target appMainVerte_generate_translations
```

The single generated header, `generated_translations.hpp`, exposes
`mv::i18n::TextId` and `Count`, a const `std::array<QString, ...>` per language
initialized with `QStringLiteral()`, and the `languages` registry of language
codes (`std::string_view`) and text spans (`std::span<const QString>`).
The dictionaries remain UTF-8; generated text literals are UTF-16 at compile
time and require Qt Core and Qt QML Integration when included:

```cpp
#include "generated_translations.hpp"

const auto text = mv::i18n::strings_en[
    static_cast<std::size_t>(mv::i18n::TextId::App_Title)];
```

The generated namespace exposes the enum to the `MainVerte` QML module using
`Q_NAMESPACE`, `Q_ENUM_NS` and `QML_NAMED_ELEMENT(TextId)`. QML can refer to IDs
as `TextId.App_Title`. The header must be processed by Qt AUTOMOC and the QML
type registrar, as configured for the app target.

Only embedded catalogs and QML enum exposure are implemented. Language selection,
Qt/QML text lookup,
parameter substitution, plural handling and external catalog loading are not
part of this generator. Existing `qsTrId()` calls do not read these catalogs.

Run generator regression tests independently of Qt:

```powershell
python -B tools/tests/test_generate_translations.py
```

They are also registered as the `translation_generator` CTest when the Qt app
and `BUILD_TESTING` are enabled.
