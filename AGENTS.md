# MainVerte

Gardening journal for Windows and Android. C++23, Qt Quick/QML, native SQLite, CMake and doctest.
Read `core/AGENTS.md` or `app_qt/AGENTS.md` when working in that subtree; both apply to changes spanning the boundary.

## Core/app contract

- Core owns business rules and persistence; app owns presentation, platform integration, database paths and Qt conversions. Dependencies point from app to Core. QML may own transient interaction state, not business invariants.
- Public Core APIs use standard C++ types and explicit error results with useful diagnostics. Keep Qt, JNI, platform APIs and SQLite handles out of public domain APIs. Put cross-step business operations in Core rather than coordinating database rules in the UI.
- `core/include/mainverte_core.h` defines the integration contract. App supplies `mv::platform` functions and owns one `mv::Services`, initialized before use and kept alive while consumers need it. Evolve this contract with its callers.
- For async APIs, specify input/result ownership, caller and callback threads, and rejection behavior. Queue acceptance is not operation success. App dispatches callbacks onto its event thread and exposes relevant failures to the user. Discarding a callback does not cancel an accepted write.

## C++ style

- Prefer direct functions, concrete types and explicit state. Add abstractions, interfaces or layers only for a demonstrated need; avoid speculative frameworks and unrelated refactoring.
- Make ownership explicit, use RAII, and use `shared_ptr` only for actual shared ownership. Preserve local conventions.
- Do not introduce `throw`, `try`/`catch`, `dynamic_cast` or `typeid` in project code. Do not change compiler flags or vendored code to enforce this policy.
- Insert a blank line after a completed control-flow block (if/else, for, while, switch) when another statement follows in the same scope. Keep attached else and do/while clauses together.

## Working agreement

- You may edit files with uncommitted changes to complete the task; preserve unrelated edits. Complete implementation and relevant verification autonomously; ask only about unresolved consequential choices beyond the authorized scope. Review/planning requests alone do not authorize edits.
- Keep changes scoped. No dependency upgrades, vendored edits or destructive repository operations unless requested, except the task-branch history updates authorized below.
- Add focused regression coverage for behavior changes. Do not disable tests, weaken warnings or change behavior merely to obtain a green build. For performance work, measure representative cases before adding complexity.

## Git and pull requests

- In Codex Cloud implementation tasks, you may commit, push a dedicated task branch and open/update a PR autonomously after relevant validation. Use at most one PR per chat, grouping all requested features and follow-up changes into it. Outside Cloud, commits and pushes require an explicit request. Never push directly to `master` or merge a PR unless requested.
- For an authorized PR workflow, record the commit where the task branch diverged from `master` (the merge base) before making changes, and keep that base fixed throughout the chat. Do not guess a base if it cannot be established.
- Before each push, consolidate only this chat's changes into one commit above that fixed base, using `git reset --soft <base>` and a new commit, or amend the existing single commit. Verify the final tree is preserved and unrelated edits/commits are excluded; never squash someone else's work. This is not a rebase onto a newer `master`.
- After starting work, do not run `git pull` or merge/rebase changes from `origin` unless explicitly requested. Leave upstream conflict resolution to the user; report conflicts without integrating upstream changes.
- Update the same PR branch on follow-ups. If rewriting an already-pushed task commit, use `--force-with-lease` with the explicit expected remote commit; stop if the branch was changed by someone else. Keep the PR title/description aligned with the complete final scope and validation results.

## Validation

- Configure from the repository root. Keep build directories separate by platform, compiler, generator and Android ABI; use the matching runtime environment for tests.
- Build affected targets and run relevant CTest cases with `--output-on-failure --no-tests=error`; build all test executables selected by the run. Use `--config Debug` / `-C Debug` for multi-config generators, or configure `-DCMAKE_BUILD_TYPE=Debug` for single-config generators.
- For CMake/dependency changes, also check a fresh configuration. Documentation-only changes need reference/diff checks, not compilation.
- Report what ran, failures and unverified behavior; distinguish pre-existing failures from regressions. Keep setup procedures in `README.md`, not these instruction files.
