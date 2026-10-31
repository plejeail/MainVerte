---
name: mainverte-qml-component
description: Create or change MainVerte QML widgets, pages and interactive behavior using its design system. Use for UI implementation, not catalog-only, SQL-only or build-only changes.
---

# MainVerte QML components

Deliver the requested interaction using existing controls and the smallest useful composition. Repository paths below are relative to the repository root.

## Choose the integration point

- For a page, inspect the closest page under `app_qt/qml/` and its navigation/caller. Keep page-specific composition local unless reuse is demonstrated.
- For shared control appearance/behavior, consult the relevant sections of `docs/design-system.md` and the matching control/helper under `app_qt/style/`. Preserve the Basic control base and its keyboard, focus, accessibility and input behavior instead of recreating it.
- Add a showcase example under `app_qt/showcase/` when it demonstrates a reusable component or significant shared state. Do not require a demo for every page edit. Keep showcase interactions independent of the journal database.

Use semantic tokens from `Tokens.qml`. Retain bindings for theme and scale changes, implicit sizing and compact layouts. Give asynchronous actions pending/error states based on Core completion; use the existing Qt bridge where sufficient. Add a bridge or Core operation only when the requested behavior needs one.

For localized text, follow the existing `Ts.get(TextId...)` usage; use the translation skill when available if catalogs or activation need changes. Do not load translation-generation internals for a layout-only task. Treat `docs/old.*` as historical ideas, not approved UX or domain requirements.

## Wire and verify

Check the module's CMake registration for new files. App QML is globbed; style/showcase lists and test module QML lists need explicit updates where the new component is consumed. Verify the generated module/resource paths rather than adding ad hoc filesystem imports.

Use `app_qt/tests/qml/tst_style.qml` and `app_qt/tests/style_test_main.cpp` for relevant UI regression coverage. The setup/language tests in `app_qt/tests/test_setup_language.cpp` cover their own integration path.

Build the affected app/test targets and run relevant CTest cases. Use the affected module's `_qmllint` target for QML changes. Exercise the changed interaction and applicable compact/enlarged-text/light/dark states; avoid exhaustive unrelated checks. Offscreen success does not establish Android touch, native rendering or deployment correctness. Report actual runtime checks and remaining gaps.
