# Persistent operations and results

Use `core/src/database.cpp` for SQL execution, `core/src/services.cpp` for public orchestration, and `core/include/mainverte_core.h` plus `definitions.h` for current contracts. Read the relevant operation and caller, not the whole persistence implementation.

- Bind values and check prepare/bind/step/reset/finalize results as appropriate. Copy SQLite text/blob results before their statement lifetime ends. Keep error context while the connection/statement still makes it available.
- Put logically atomic writes and associated selection/settings changes in one Core operation and transaction. Use the existing transaction/error patterns, checking completion and rollback. Do not report success after only part of the operation persisted.
- Initialize synchronously, then execute database work on the existing worker. Keep captures alive through execution; do not retain a borrowed SQLite handle or add a parallel connection.
- Separate submit acceptance from completion. Inspect each API's actual invalid-input/refusal behavior rather than assuming every false return invokes a callback. Use `mv::platform::dispatch_callback` for public delivery and update the in-memory settings cache on its owner thread only after persistence succeeds.
- Adapt the Qt caller only if the requested operation needs it: standard C++ owned results become Qt state there; SQL/business rules stay in Core.

Use `core/tests/test_create_gardener.cpp` as a public-operation example and `test_database_thread.cpp` for queue/lifetime behavior. Test observable stored data, atomicity, errors and relevant callback semantics; use deterministic synchronization rather than sleeps. Keep doctest assertions on the test thread when worker results can be returned as data.
