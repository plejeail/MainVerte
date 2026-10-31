---
name: mainverte-build-diagnostics
description: Diagnose MainVerte CMake, compiler, linker, runtime-loading or Android deployment failures. Use for build/environment problems, not routine successful validation or functional test assertions.
---

# MainVerte build diagnostics

Find the failing stage and demonstrate its cause before changing configuration. Repository paths below are relative to the repository root.

Inspect the first useful error and the relevant cache fields: generator, build configuration, compiler, Qt prefix, toolchain and Android ABI. Use [stage-specific checks](references/stages.md) only for the failing stage. Existing build folders and machine paths are evidence, not portable defaults.

Reproduce with the smallest affected target or test. Inspect `README.md` and the relevant CMake files for intended setup; keep unrelated warnings distinct from the failure. Use current official Qt/CMake/Android documentation when an API or toolchain requirement is uncertain.

Preserve existing caches; use a separate build directory for a new kit, generator or clean-configuration comparison. Do not delete source/user data, weaken warnings, disable tests, upgrade dependencies or rewrite build rules to hide an environment problem. The skill does not authorize those scope changes.

After a supported fix, rerun the failing operation and relevant checks. Stop repeating the same failure when missing SDKs, runtimes or device access require an external change; report the exact blocker and completed checks. Summarize cause, change and evidence, including whether Android was actually built/deployed.
