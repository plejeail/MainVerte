# Core

## Persistence and services

- Keep Core buildable without Qt or a platform SDK. Use the SQLite C API; do not add Qt SQL, an ORM or platform-dependent services. Use the app-supplied path rather than a working-directory or machine-specific path.
- `mv::Services` controls internal service lifetime. Initialize and tear down on the owner thread with no concurrent service access; internal consumers borrow services, not their ownership.
- SQLite uses `SQLITE_THREADSAFE=0`. Production uses one `Database`: synchronous initialization/settings loading on its owner thread, then serialized jobs on its worker. Do not introduce concurrent SQLite access, including through another connection.
- Keep SQLite handles inside persistence implementation. Use prepared statements and bound values, RAII for connections/statements, and check SQLite results, including transaction completion. Enable and verify foreign keys.
- Add versioned migrations; never rewrite shipped migrations or recreate a user's database. Preserve data and atomicity. See `../README.md`'s SQL migrations section when changing the schema; cover fresh creation, upgrades with existing data and failure/rollback.
- Worker jobs own data needed until completion. Deliver public results through `mv::platform::dispatch_callback`; update the settings cache on the owner thread. Preserve documented rejection/completion semantics.
- Stop producers before teardown; drain accepted jobs and join the worker. The app dispatcher must remain available until Core finishes producing callbacks.

## Validation

Configure from the repository root with `-DBUILD_QT_APP=OFF -DBUILD_TESTING=ON` for Core-only validation; `core/` is not a standalone CMake project. Build `mainverte_core_tests` and run the `^mainverte_core_tests$` CTest case. Keep persistence tests isolated from user data.
