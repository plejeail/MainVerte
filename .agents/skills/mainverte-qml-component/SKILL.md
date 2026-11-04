---
name: mainverte-qml-component
description: Implement or modify MainVerte QML pages, controls and interactive UI behavior. Use for QML interface changes, not translation-only, SQL-only or build-diagnostics tasks.
---

# MainVerte QML components

Implement the requested interaction within the existing Qt/QML architecture.
Follow `app_qt/AGENTS.md` and inspect only the relevant code and references.

## Design and composition

- Inspect the closest existing page or control and its callers before adding components. Keep page-specific behavior local; extract shared components when reuse is demonstrated.
- For visual changes, consult `docs/design-system.md` and the relevant controls under `app_qt/style/`.
- Use semantic tokens from `Tokens.qml`. Preserve theme bindings, implicit sizing, compact layouts and system text scaling.
- Prefer existing Qt Quick Controls and preserve keyboard, focus, accessibility and input behavior.
- Treat `docs/old.*` as historical material, not current UX requirements.

## Integration

- Use existing Qt bridges and Core APIs. Introduce new integration mechanisms only when required by the behavior.
- Reflect asynchronous operation states accurately: pending, success and failure must follow actual Core completion.
- Use `Ts.get(TextId...)` for localized text. Invoke `mainverte-translations` when modifying catalogs or translation behavior.
- Check CMake QML registration for new components. App QML files are globbed; style, showcase and test modules require explicit registration where applicable.
- Add a showcase example for reusable controls or significant shared interactions, not routine page changes. Keep showcases independent of the journal database.

## Verification

- Add focused regression coverage where behavior changes. Reuse `app_qt/tests/qml/tst_style.qml`, `style_test_main.cpp` or the closest relevant tests.
- Build affected targets and run relevant CTest cases and QML lint targets.
- Exercise affected interactions, including relevant compact layout, enlarged text and light/dark states.
- Distinguish offscreen test results from actual desktop or Android verification. Report unverified behavior without claiming platform coverage.