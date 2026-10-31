# Text and language changes

Inspect the ordered `DICTIONARIES` list in `app_qt/CMakeLists.txt` and the relevant `app_qt/translations/*.dict` entries.

- Catalogs are UTF-8 CSV without a header, with two columns: C++ identifier and text. Preserve CSV quoting for commas, quotes and multiline values.
- The first dictionary defines the ID order. Every dictionary must have exactly the same ID set; other file orders may differ. Duplicates, reserved C++ identifiers and `Count` are invalid. Empty translations are permitted by the generator; do not introduce them as silent substitutes for requested translations.
- QML uses `Ts.get(TextId.Some_Id)`; C++ uses `ts::get(ts::TextId::Some_Id)` with `translator.h`. Include accessibility text when it is part of the new control. These are app-layer APIs.
- For a new language, add its dictionary to `DICTIONARIES` and inspect `ts::language_options()` and app/test flag resources: current options construct `qrc:/img/flag_<code>.svg`. Update relevant resource lists and language-count/order assumptions in tests. Do not assume a dictionary alone completes language integration.

Build `appMainVerte_generate_translations` in the selected app build directory to validate catalog generation. For activation or a new language, build `mainverte_setup_tests` and run the relevant `setup_language_*` cases; inspect their current registration in `app_qt/CMakeLists.txt`. Exercise new-language text in the UI when available.
