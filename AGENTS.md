## Project and boundaries

- MainVerte is a gardening journal for Android (ARM64/x86_64) and Windows desktop.
- Stack: C++23, Qt >= 6.12, Qt Quick/QML, CMake >= 3.25, native SQLite C API, doctest.
- `core/include/`: public Core API; `core/src/`: implementation; `core/tests/`: doctest tests via CTest.
- `app_qt/src/`: Qt application and platform services; `app_qt/qml/`: QML UI (module `MainVerte`).
- `third_party/`: vendored SQLite and doctest. Targets: `mainverte_core`, `mainverte_core_tests`, `appMainVerte`.

## Non-negotiable architecture

- Core owns business logic and persistence; it must remain independent of Qt, Android, Windows and JNI.
- `app_qt/` owns presentation, platform integration, paths and conversion to/from Qt types. Dependencies point from app to Core, never backward.
- QML can own transient visual state and local interactions, not database rules or business invariants.
- App supplies platform-specific inputs such as the database path. Introduce a service interface only for a demonstrated need.
- Access SQLite via its C API; contain `sqlite3*` and `sqlite3_stmt*` in persistence implementation, not public domain APIs.
- SQLite is a permitted Core dependency; doctest is test-only. Do not add Qt SQL, an ORM, or other dependencies without concrete justification.
- Target state: Core builds and tests without Qt or Android SDK. **Present limitation:** root CMake unconditionally adds `app_qt/`, and `core/` is not a standalone CMake project. Do not claim standalone builds work, invent presets/options, or fix this outside an assigned task.

## Engineering style

- Prefer direct code, local reasoning, explicit state and simple data structures. Keep the set of possible behaviors small.
- Avoid speculative abstractions, general frameworks, SOLID/DDD/Clean Architecture boilerplate, dependency-injection containers and interfaces with one implementation.
- Add classes, inheritance, polymorphism and layers only when solving a concrete problem. Prefer ordinary functions and concrete types when sufficient.
- Make ownership and lifetime explicit; use RAII for SQLite connections, statements and other resources. Do not introduce `shared_ptr` without a real shared-ownership requirement. Respect Qt/QML object ownership.
- **Code policy:** do not introduce `throw`, `try`/`catch`, `dynamic_cast` or `typeid` into project Core or app code. Exceptions and RTTI remain enabled at compile time for now; do not change build flags or vendored code to enforce this policy.
- Use explicit error results and preserve useful diagnostic context. Check SQLite return codes. Do not use blanket `noexcept` on potentially throwing call chains.
- Core returns errors; the app translates relevant failures into visible Qt/QML state or user notifications. No UI messages or Qt signals in Core. For async failures, define who receives the result and on which thread.
- Prefer correctness and clarity over speculative micro-optimizations. Measure before adding performance complexity.
- Follow surrounding naming, formatting and coding conventions; do not reformat unrelated code.

## SQLite and persistent data

- Use prepared statements and bound parameters for values. Finalize statements and close connections on every path; use RAII.
- App resolves the database location; do not rely on working directory or developer-machine absolute paths.
- Schema upgrades must be versioned migrations, preserve existing user data and have tests. Use transactions for multi-step atomic changes; check commit/rollback results.
- Enable and verify foreign-key enforcement on each connection where required.
- The intended SQLite design is single-threaded (`SQLITE_THREADSAFE=0`). Never silently introduce concurrent SQLite access; document ownership and thread affinity before adding asynchronous work.
- Keep long-running work off the Qt UI thread without introducing unused threading infrastructure.
- Never delete or recreate a user's existing database as a migration shortcut.

## Working agreement

- Before edits: read relevant code, callers and applicable nested `AGENTS.md`; inspect `git status` and preserve uncommitted changes.
- For research, review or planning requests, do **not** edit files. For implementation requests, make the smallest coherent change that solves the complete task.
- Infer reversible local implementation details; ask before choosing semantics that alter user-visible behavior, public API compatibility or persistent data.
- Do not refactor, rename, format or upgrade dependencies outside the requested scope. Do not edit vendored third-party code unless specifically asked.
- Do not silently weaken compiler warnings, tests or build settings to obtain a green build.
- Add focused regression tests for behavior changes and bug fixes. Prefer runnable evidence to assertions that a change is correct.
- Before handoff, inspect the diff and run relevant checks. Report what actually ran, what failed, what was not verified and why. Distinguish existing failures from introduced ones.
- No commits, pushes or destructive repository operations unless explicitly requested.

## Build and verification

Use separate CMake build directories for each platform, compiler and Android ABI. Never reuse a CMake cache across toolchains. Execute commands from repository root, with a suitable Qt >= 6.12 kit configured.

Desktop with a multi-config generator:

```powershell
cmake -S . -B build/desktop -DBUILD_TESTING=ON
cmake --build build/desktop --config Debug --target appMainVerte mainverte_core_tests
ctest --test-dir build/desktop -C Debug --output-on-failure --no-tests=error
```

For a single-config generator, configure with `-DCMAKE_BUILD_TYPE=Debug` and omit `--config Debug` / `-C Debug`. If Qt is not found, supply the appropriate kit prefix through `CMAKE_PREFIX_PATH`; never commit local paths.

Validation expectations:

- **Core/business logic:** build Core, execute relevant doctest/CTest cases, and add useful coverage for bugs. An empty test suite is not evidence.
- **Qt/QML:** build app and exercise affected UI when possible. If UI cannot be run, say so.
- **CMake/dependencies:** verify fresh configuration and affected targets; do not mask errors with configuration changes.
- **Android:** use the actual Android kit/ABI; report ABI, build and device/emulator checks. Desktop success does not establish Android success.
- **Documentation only:** inspect diff, references and commands; compilation generally unnecessary.

## Documentation

- This file contains durable repository policy, architecture constraints and known commands—not a task log, roadmap or generic C++ tutorial.
- Keep SDK/NDK installation, onboarding and extensive deployment procedures in `README.md` or dedicated documents.
- Update documentation when changing a documented command or constraint; report mismatches beyond task scope rather than silently expanding work.
