# Qt application

## Qt/QML and platform integration

- Consume Core's public API; do not reach into `core/src/` or access the database directly. Resolve the database path with Qt's application data facilities.
- Respect QObject/QML ownership and thread affinity. Queue Core completions onto the Qt event thread; guard callbacks targeting objects that may disappear, and turn relevant errors into visible UI state. Keep blocking work off the UI thread.
- Keep the dispatcher alive through Core teardown, then drain or explicitly abandon pending callbacks before stopping its event loop.
- Keep JNI and Android APIs in platform-specific app code. Android's UI thread is distinct from Qt's event thread: dispatch Android View/Window operations appropriately and do not carry unguarded Qt object references across threads. Check API availability against the supported Android versions.

## UX

- For visual changes, use `../docs/design-system.md`: reuse semantic tokens and existing controls, preserve Qt Quick Controls interaction/accessibility behavior, and support compact layouts, light/dark themes and system text scaling.
- Keep persistent settings and business validation in Core; QML owns presentation and transient interactions. Show pending, success and failure states consistently with actual Core completion.
- Use the existing translation mechanism for user-facing text; change dictionaries/generator inputs rather than generated headers. See `../README.md`'s embedded translations section when needed.

## Validation

- Use the matching Qt kit to build `appMainVerte` and affected tests with `BUILD_TESTING=ON`. Run relevant CTest cases; use the QML lint targets and UI checks appropriate to the change.
- Offscreen tests do not establish native rendering, input or deployment correctness. Exercise affected UI on the target platform when available and report gaps.
- Android changes require the actual Android kit/ABI; report build and device/emulator checks separately. Desktop success does not validate Android or JNI behavior.
