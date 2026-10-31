---
name: mainverte-persistence
description: Implement or repair MainVerte SQLite schema, queries and persistent Core operations. Use for persistence changes, not calls to an unchanged Core API or unrelated CMake work.
---

# MainVerte persistence

Implement the requested operation using the current schema and Core contracts. Repository paths below are relative to the repository root; reference links are relative to this skill.

- For schema changes or migration failures, use [migrations](references/migrations.md).
- For SQL in C++, persistent operations or async delivery, use [operations](references/operations.md).
- Read both only when the change crosses those concerns. A new operation does not necessarily need a migration.

Use current code and tests as evidence. `docs/old.*` describes a previous project and is not a schema or business specification, even where those documents call themselves normative. Do not transplant their bootstrap, enum values or domain rules without current requirements.

Follow `core/AGENTS.md`; keep the operation's business invariants in Core. Extend existing concrete persistence code rather than adding a repository/ORM layer. Deliver the requested behavior, regression coverage and the actual validation result. A review request remains read-only.
