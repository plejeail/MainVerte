# Schema and embedded migrations

Inspect `core/sql/`, `tools/cmake_generate_sql.cmake`, and the migration code in `core/src/database.cpp`. Use the SQL migrations section of `README.md` for the format.

- Files have consecutive zero-based numeric prefixes with consistent padding. CMake sorts them lexically and embeds matching files in the build-tree `core/generated/sql_scripts.hpp`; the script count defines the target database version.
- Script index N upgrades `PRAGMA user_version` from N to N+1. Core owns the transaction and version update for each script. Keep transaction boundaries and version changes out of migration SQL.
- Add the next migration for an existing schema change; do not modify shipped scripts. Preserve previous data and reject unsupported downgrades. Earlier successful migrations remain committed if a later one fails.
- SQL content changes trigger CMake regeneration. Fix the source script or embedding rule, not the generated header. Change the generator only if its behavior actually needs to change.

For upgrade coverage, follow `core/tests/test_database_migration.cpp`: build a previous schema with representative data, apply the new migration and check both data and version. Also verify fresh creation and relevant constraints/failure rollback. Use lifecycle/configuration tests when initialization is affected. Do not open a second SQLite connection while a database worker is active.

Build `mainverte_core_tests` and run its CTest case in a matching kit; Core-only configuration is documented in `core/AGENTS.md`. Reconfigure when verifying an embedding change.
