# MainVerte agent instructions

## Project map

- Gardening app: Android ARM64/x86_64 and Windows Desktop. C++23, Qt >= 6.12, Qt Quick/QML, CMake >= 3.25.
- `core/include/`: public API; `core/src/`: implementation; `core/tests/`: doctest tests registered with CTest.
- `app_qt/src/`: Qt app and platform integration; `app_qt/qml/`: UI, QML module `MainVerte`.
- `third_party/`: native SQLite C library and doctest. Main targets: `mainverte_core`, `mainverte_core_tests`, `appMainVerte`.

## Architecture

- Keep Core Qt/platform independent: no Qt types in its API; no direct Windows, Android or JNI API calls. App depends on Core, never the reverse. SQLite is an allowed Core dependency; doctest is test-only.
- Core owns business logic and persistence. `app_qt/` owns presentation, Qt conversions and platform services. QML may handle visual state and local interactions.
- App supplies environment-specific inputs, including the database path. Add service functions/interfaces only for concrete needs.
- Use SQLite's C API; keep handles and implementation details inside persistence code. Justify any Qt SQL or ORM introduction.
- Required end state: configure, build and test Core with SQLite/doctest without Qt or mobile SDKs. Explicit portable dependencies are compatible with Core independence. See current build limitation below.

## Build and validation

Run from repository root with compiler and Qt >= 6.12 kit configured. Use separate build directories per kit/platform/ABI; never reuse a cache across toolchains.

Desktop, multi-configuration generator:

```powershell
cmake -S . -B build/desktop -DBUILD_TESTING=ON
cmake --build build/desktop --config Debug --target appMainVerte mainverte_core_tests
ctest --test-dir build/desktop -C Debug --output-on-failure --no-tests=error
```

Single-configuration generator: add `-DCMAKE_BUILD_TYPE=Debug` when configuring; omit `--config Debug` and `-C Debug`. If Qt discovery fails, set `CMAKE_PREFIX_PATH` to the installed kit prefix; never commit personal paths.

**Current limitation:** root CMake always adds `app_qt/`, requiring Qt even when building only Core. `core/` is not a standalone CMake project. Do not claim Qt-free validation or invent options/presets. Fix this when tasked with Core build independence; do not expand unrelated work.

| Change | Required checks |
|---|---|
| Business logic | Build Core; run relevant tests. Add useful regression coverage for bug fixes; an empty suite does not validate behavior. |
| Qt/QML | Build app; exercise affected interaction when possible. Report if UI was not run. |
| CMake/dependencies | Check fresh configuration and affected targets; do not change options to hide failures. |
| Android | Use matching kit/toolchain; report ABI built and device/emulator checks. Desktop builds do not validate Android. |
| Documentation only | Check diff, paths and command consistency; no build required. |

## Change workflow

- Before editing: read applicable instructions, inspect `git status`, affected code and callers. Check for more specific `AGENTS.md` instructions before working in subdirectories.
- Preserve existing changes, including uncommitted work; never revert/overwrite developer work for convenience.
- Make the smallest coherent change that fully satisfies the request. Avoid unrelated refactoring, renaming and formatting.
- Reuse existing mechanisms. Justify new dependencies with concrete need and platform impact. Modify third-party sources only when explicitly tasked.
- Decide reversible local details autonomously; clarify unresolved choices affecting expected behavior, compatibility or persistent data.
- Review final diff and run relevant checks. Distinguish introduced failures from pre-existing issues; do not silently expand scope to fix the latter.
- Report outcome, checks actually run and unverified items with reasons. Proposed commands are not completed checks.

## Coding conventions

- Follow consistent local style. Prefer simple data structures and direct functions. Add classes, polymorphism or layers only for identified needs; do not apply SOLID, DDD or patterns by default.
- Use RAII, including SQLite connections/statements. Make ownership/lifetimes explicit across Core/Qt/QML; respect Qt ownership in the app.
- Do not introduce no `throw`, `try`/`catch`, `dynamic_cast` or `typeid` in app/Core code. Do not modify third-party libraries to enforce this.
- Return explicit error codes, with result/context as needed; check SQLite return values. Avoid blanket `noexcept`: called operations may still throw.
- Return synchronous errors to callers; use explicit notifications for asynchronous failures when needed. App converts results into messages, Qt/QML state or user notifications. Core neither uses Qt signals nor displays errors.
- Make new CMake includes, dependencies and options target-scoped; justify `PRIVATE`/`PUBLIC` visibility.
- Add performance complexity only for an identified constraint; measure when optimization motivates a change.

## Platforms and persistence

- Isolate platform differences in the app, near the affected service. Resolve storage locations there; do not rely on the working directory or absolute development paths.
- Keep long operations off the UI thread. When introducing async work, define lifetimes, notification thread and SQLite connection access without unnecessary infrastructure.
- Schema changes require explicit migration and tests preserving existing data. Bind SQL values; use transactions when multiple writes must be atomic.

## Documentation and scope

- Keep durable rules/common commands here. Put setup, SDK/NDK kits, deployment and lengthy procedures in `README.md` or referenced technical documents when available.
- Keep task goals/acceptance criteria in the request or issue; this file is neither a session log nor a roadmap.
- Update relevant documentation when changing a documented command/constraint. Report intent/implementation mismatches without starting out-of-scope migrations.
