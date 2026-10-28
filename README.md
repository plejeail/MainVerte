# MainVerte gardening app

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
cmake --build build/desktop-x64-debug --target appMainVerte_generate_translations
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
