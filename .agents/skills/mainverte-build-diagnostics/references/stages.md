# Diagnose the failing stage

## Configure or generate

Compare the selected kit with root/app CMake requirements. Core-only work can use `BUILD_QT_APP=OFF`; it is not a workaround for an app failure. App translation generation needs host Python; tests need Qt Quick Test from the same kit. Check `CMAKE_PREFIX_PATH`, compiler and generator in the actual cache rather than guessing from folder names.

## Compile or generate sources

For embedded SQL, inspect `tools/cmake_generate_sql.cmake`; for translations, inspect `tools/translate.cmake` and its Python command. Fix source inputs rather than generated headers. Request a verbose build for the failing command when flags or include paths are unclear.

For QML cache generation, inspect the generated qmlcachegen command. Current Debug-only bytecode settings use `CMAKE_BUILD_TYPE`; Qt 6.12 passes generator expressions in `QT_QMLCACHEGEN_ARGUMENTS` literally. Verify Debug and Release commands before changing this condition; do not globally disable QML compilation to mask the error.

## Link

Distinguish a missing library, unresolved symbol, incompatible architecture and static QML plugin/resource omission. Check target dependencies and the actual link command before adding global link paths or recompiling vendored dependencies.

## Launch or CTest

Use `ctest --test-dir <build-dir> -N` to inspect selected registrations and confirm their executables were built. Match `--config` / `-C` for multi-config builds. CTest does not build tests.

On Windows, `0xc0000135` is a loader failure, not a test assertion. Inspect runtime dependencies and use Qt/compiler DLLs from the matching kit in the launch environment. For QML plugin/import failures, inspect embedded modules, resources and engine diagnostics; do not copy arbitrary DLLs into source control. Compare with an existing unchanged build only when its kit and configuration make the comparison meaningful.

If direct execution succeeds but CTest reports `0xc0000135`, compare the child process environments. An inherited environment can contain both `PATH` and `Path`; PowerShell's `$env:PATH` alone may hide the duplicate. Inspect the raw process environment, for example with `[Environment]::GetEnvironmentVariables('Process').GetEnumerator()`, filtering keys case-insensitively. In this project, CTest retained the entry without the Qt/MinGW runtime directories; switching CTest versions did not reliably fix all tests. Confirm the duplicate and normalize to one entry in the launch session, preserving the required runtime directories, then rerun CTest. See `README.md` for the session setup commands; do not infer missing DLL installations or change the user's persistent environment from this symptom alone.

Once loading succeeds, treat subsequent Qt warnings separately. For offscreen tests, a missing kit `lib/fonts` warning can fail tests that use `failOnWarning`; use the installed Windows fonts through the session's `QT_QPA_FONTDIR` as described in `README.md`, rather than suppressing the warning. If a GUI test fails without console output, request a QtTest text report with `-o <report-path>,txt` to reveal the actual failure.

## Android package or deploy

Inspect the actual Qt Android kit, host tools, NDK, SDK, JDK and target ABI. Distinguish C++ build from packaging, installation and application startup; use the earliest relevant Gradle/androiddeployqt/adb error. Check device/emulator ABI and runtime logs when available. Keep local SDK paths out of tracked configuration. A desktop build or a host test with Android stubs does not verify JNI or Android execution.
